#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>
using namespace std;

#ifdef DEBUG
#define ASSERT(a) assert(a)
#else
#define ASSERT(a)
#endif

namespace game20x {

constexpr int DEFAULT_SAMPLE_PER_NODE_PER_PLAYER = 8;
constexpr int MAX_NODE_COUNT = 256 * (1 << 10);

using VType = int;
using Float = double;
using Byte = unsigned char;

constexpr VType MAX_V = 10000;
constexpr VType MIN_V = -10000;

enum class Player {
    P0 = 0,
    P1 = 1,
    P_END = 2
};

constexpr int N_PLAYER = static_cast<int>(Player::P_END);
constexpr int GLOBAL_CHAR_BUFFER_SIZE = 10 * 1024;

char gCharBuffer[GLOBAL_CHAR_BUFFER_SIZE];

template<typename K, typename T, class Hash = hash<K>>
using Map = unordered_map<K, T, Hash>;

template<class T>
struct Hasher {
    int operator()(const T& t) const { return t.hash(); }
};

template<typename K, typename T>
using HashMap = Map<K, T, Hasher<K>>;

template<class T>
struct PairHasher {
    int operator()(const pair<T, T>& pair) const {
        return pair.first.hash() * pair.second.MAX_HASH + pair.second.hash();
    }
};

template<typename K, typename T>
using PairHashMap = Map<pair<K, K>, T, PairHasher<K>>;

template<class Action, class State, int SAMPLE_COUNT = DEFAULT_SAMPLE_PER_NODE_PER_PLAYER>
class SearchNode {
public:
    void init(const State& state) {
        gameState = state;
        totalFreq[0] = totalFreq[1] = totalPositiveRegret[0] = totalPositiveRegret[1] = 0;
        children.clear();
        regrets[0].clear();
        regrets[1].clear();
        actionFreq[0].clear();
        actionFreq[1].clear();
#ifdef DEBUG
        sampleUtilitySum = 0;
        sampleUtilityCnt = 0;
#endif
    }

    static int MemoryIndex;
    static SearchNode* Memory;
    static SearchNode* Make(const State& state) {
        SearchNode* result = Memory + (MemoryIndex++);
        result->init(state);
        return result;
    }
    static void ClearMemory() {
        MemoryIndex = 0;
    }

    vector<Action> sampleRandomActions(Player player, int count = SAMPLE_COUNT) {
        if (count >= gameState.getAllActionCount(player)) {
            return gameState.getAllActions(player);
        } else {
            vector<Action> results;
            for(int i = 0; i < count; ++i)
                results.push_back(gameState.sampleRandomAction(player));
            return results;
        }
    }

    template<typename FType>
    Action sampleAction(Player player, const HashMap<Action, FType>& sampleFreq,
            const FType totalFreq, bool needsDump = false) {
        if (totalFreq == 0)
            return gameState.sampleRandomAction(player);
        int rn = rand() % totalFreq;
        if (needsDump)
            cerr << "rn: " << rn << ", totalFreq: " << totalFreq << endl;
        int t = 0;
        for(auto it = sampleFreq.cbegin(); it != sampleFreq.cend(); ++it) {
            t += max<FType>(0, it->second); // freq might be negative for cfr
            if (needsDump)
                cerr << "  action: " << it->first.dumps() << ", t: " << t << endl;
            if (t > rn) {
                return it->first;
            }
        }
        ASSERT(false); // we shouldn't reach here
        return Action();
    }

    inline Action sampleFinalAction(Player player) {
        return sampleAction(player, actionFreq[(int)player], totalFreq[(int)player], true);
    }

    inline void addActionFreq(Player player, const Action& action) {
        totalFreq[(int)player]++;
        actionFreq[(int)player][action]++;
    }

    inline void addActionRegret(Player player, const Action& action, VType delta) {
        VType& regret = regrets[(int)player][action];
        VType newRegret = regret + delta;
        totalPositiveRegret[(int)player] -= max<VType>(0, regret);
        totalPositiveRegret[(int)player] += max<VType>(0, newRegret);
        regret = newRegret;
    }

    inline SearchNode& getChild(const Action& a0, const Action& a1) {
        pair<Action, Action> actionPair(a0, a1);
        if (children.find(actionPair) == children.end()) {
            children.emplace(actionPair, SearchNode::Make(gameState.next(a0, a1)));
        }
        // {
        //     // TODO TEST
        //     SearchNode& node = *children.find(actionPair)->second;
        //     if (!(node.gameState.a0[0] == a0.a && node.gameState.a1[0] == a1.a)) {
        //         node.dump(cerr, 0, 0);
        //         assert(false);
        //     }
        // }
        return *children.find(actionPair)->second;
    }

    VType sampleUtility(int depthLimit, bool needsDump = false) {
        if (depthLimit == 0 || gameState.isTerminal()) {
            return gameState.estimateU(needsDump);
        }
        Action a0 = sampleAction(Player::P0, regrets[0], totalPositiveRegret[0]);
        Action a1 = sampleAction(Player::P1, regrets[1], totalPositiveRegret[1]);

        VType result = getChild(a0, a1).sampleUtility(depthLimit - 1, needsDump);
#ifdef DEBUG
        sampleUtilitySum += result;
        sampleUtilityCnt++;
#endif
        return result;
    }

    VType visit(int depthLimit) {
        if (depthLimit == 0 || gameState.isTerminal()) {
            return gameState.estimateU();
        }
        Action a0 = sampleAction(Player::P0, regrets[0], totalPositiveRegret[0]);
        Action a1 = sampleAction(Player::P1, regrets[1], totalPositiveRegret[1]);
        addActionFreq(Player::P0, a0);
        addActionFreq(Player::P1, a1);
        State nextState = gameState.next(a0, a1);
        VType utility = getChild(a0, a1).visit(depthLimit - 1);

        vector<Action> samples0 = sampleRandomActions(Player::P0);
        vector<Action> samples1 = sampleRandomActions(Player::P1);

        for(const auto& aa0 : samples0) {
            // if (aa0.dumps() == "0" && a1.dumps() == "17") { // TODO TEST
            //     SearchNode& child = getChild(aa0, a1);
            //     PairHasher<Action> hasher;
            //     pair<Action, Action> altpair(aa0, a1);
            //     pair<Action, Action> statepair(child.gameState.a0[0], child.gameState.a1[0]);
            //     cerr << "altpair hash = " << hasher(altpair) << ", statepair hash = " << hasher(statepair) << endl;
            //     cerr << "child a0, a1 = " << child.gameState.a0[0] << ", " << child.gameState.a1[0] << endl;
            //     sprintf(gCharBuffer, "a0 = %d, a1 = %d, alt utility = %d, utility = %d, regret = %d\n",
            //             a0.a, a1.a, child.sampleUtility(depthLimit), utility, regrets[0][aa0]);
            //     cerr << gCharBuffer;
            // }
            addActionRegret(Player::P0, aa0,
                    getChild(aa0, a1).sampleUtility(depthLimit) - utility);
        }
        for(const auto& aa1 : samples1) {
            // player 1's utility is negated
            addActionRegret(Player::P1, aa1,
                    -(getChild(a0, aa1).sampleUtility(depthLimit) - utility));
        }
#ifdef DEBUG
        sampleUtilitySum += utility;
        sampleUtilityCnt++;
#endif
        return utility;
    }

    void dump(ostream& os, int depth, int maxDepth = -1) {
        if (maxDepth > -1 && depth > maxDepth)
            return;
        string indent(depth * 2, ' ');
        os << indent << "SearchNode:" << endl;
        os << indent << "  gameState: " << gameState.dumps() << endl;
#ifdef DEBUG
        os << indent << "  sampled utility: " << sampleUtilitySum << '/' <<
                sampleUtilityCnt << " = " << (double)sampleUtilitySum / sampleUtilityCnt << endl;
        os << indent << "  estimate utility: " << gameState.estimateU() << endl;
        os << indent << "  lowerV <= upperV : " << lowerV() << " <= " << upperV() << endl;
#endif
        for(int player = 0; player < 2; ++player) {
            os << indent << "  actionFreq of player " << player << ":" << endl;
            for(const auto& it : actionFreq[player]) {
                os << indent << "    " << it.second << "(" << (Float)it.second / totalFreq[player]
                        << "): " << it.first.dumps() << endl;
            }
            os << indent << "  regrets of player " << player << ":" << endl;
            for(const auto& it : regrets[player]) {
                os << indent << "    " << it.second << ": " << it.first.dumps() << endl;
            }
        }
        os << indent << "  children:" << endl;
        for(auto& it : children) {
            it.second->dump(os, depth + 1);
        }
        os << indent << "SearchNode ends" << endl;
    }

    void dumpRegrets0(ostream& os, int depthLimit) {
        os  << "ActionFreq of player 0:" << endl;
        for(const auto& it : actionFreq[0])
            os << it.second << "(" << (Float)it.second / totalFreq[0]
                    << "): " << it.first.dumps() << endl;
        os << "Regrets of player 0:" << endl;
        for(const auto& it : regrets[0]) {
            os << it.second << ": " << it.first.dumps() << endl;
        }
    }

    void smallDump(ostream& os, const Action* a0, const Action* a1, int depth, int maxDepth = -1) {
        if (depth > maxDepth)
            return;
        os << "depth: " << depth << ", gameState: " << gameState.dumps() << endl;
        os << "estimateU: " << gameState.estimateU() << endl;
        Action sa0 = sampleAction(Player::P0, regrets[0], totalPositiveRegret[0]);
        Action sa1 = sampleAction(Player::P1, regrets[1], totalPositiveRegret[1]);
        if (a0 == nullptr || a1 == nullptr) {
            a0 = &sa0;
            a1 = &sa1;
        }
        os << "a0: " << a0->dumps() << endl;
        os << "a1: " << a1->dumps() << endl << endl;
        getChild(*a0, *a1).smallDump(os, nullptr, nullptr, depth + 1, maxDepth);
    }

protected:
    // TODO will cumulative regret exceed int range?
    HashMap<Action, VType> regrets[N_PLAYER]; // from action id (distinct between both players) to cfr
    VType totalPositiveRegret[N_PLAYER];

    HashMap<Action, int> actionFreq[N_PLAYER]; // the frequency an action is played for our final sampling
    int totalFreq[N_PLAYER];

    State gameState;

    PairHashMap<Action, SearchNode*> children;

    // P0 for lowerV (P0 first)
    // P1 for upperV (P1 first)
    Float v(Player player) {
        if (gameState.isTerminal()) {
            return gameState.estimateU();
        }

        int sign = player == Player::P0 ? 1 : -1;
        Player otherPlayer = (Player)(1 - (int)player);

        if (actionFreq[(int)player].empty()) {
            for(const auto& action : sampleRandomActions(player))
                addActionFreq(player, action);
        }

        vector<Action> otherActions = sampleRandomActions(otherPlayer);
        HashMap<Action, Float> vMap;
        for(const auto& otherAction : otherActions)
            vMap[otherAction] = 0;
        for(const auto& it : actionFreq[(int)player]) {
            const auto& thisAction = it.first;
            Float p = (Float)it.second / totalFreq[(int)player];
            for(const auto& otherAction : otherActions) {
                const auto& a0 = player == Player::P0 ? thisAction : otherAction;
                const auto& a1 = player == Player::P0 ? otherAction : thisAction;
                vMap[otherAction] += p * getChild(a0, a1).v(player);
            }
        }

        Float result = MAX_V;
        for(const auto& otherAction : otherActions)
            result = min(result, vMap[otherAction] * sign);

        return result * sign;
    }

    Float lowerV() { return v(Player::P0); }
    Float upperV() { return v(Player::P1); }

#ifdef DEBUG
    VType sampleUtilitySum;
    int sampleUtilityCnt;
#endif
};

}
//////////// namespace game20x ends //////////////////////////
