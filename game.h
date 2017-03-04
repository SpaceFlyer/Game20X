#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>
using namespace std;

namespace game20x {

constexpr int DEFAULT_SAMPLE_PER_NODE_PER_PLAYER = 8;

using VType = int;
using Float = double;
using Byte = unsigned char;

constexpr VType MAX_V = 1e8;
constexpr VType MIN_V = -1e8;

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

template<class Action, class State>
class SearchNode {
public:
    SearchNode(const State& state) : gameState(state) {
        totalFreq[0] = totalFreq[1] = totalPositiveRegret[0] = totalPositiveRegret[1] = 0;
        sampleUtilitySum = 0;
        sampleUtilityCnt = 0;
    }

    vector<Action> sampleRandomActions(Player player, int count = DEFAULT_SAMPLE_PER_NODE_PER_PLAYER) const {
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
            const FType totalFreq) const {
        if (totalFreq == 0)
            return gameState.sampleRandomAction(player);
        int rn = rand() % totalFreq;
        int t = 0;
        for(auto it = sampleFreq.cbegin(); it != sampleFreq.cend(); ++it) {
            t += max<FType>(0, it->second); // freq might be negative for cfr
            if (t > rn) {
                return it->first;
            }
        }
        assert(false); // we shouldn't reach here
        return Action();
    }

    void addActionFreq(Player player, const Action& action) {
        totalFreq[(int)player]++;
        actionFreq[(int)player][action]++;
    }

    void addActionRegret(Player player, const Action& action, VType delta) {
        VType& regret = regrets[(int)player][action];
        VType newRegret = regret + delta;
        totalPositiveRegret[(int)player] -= max<VType>(0, regret);
        totalPositiveRegret[(int)player] += max<VType>(0, newRegret);
        regret = newRegret;
    }

    SearchNode& getChild(const State& state) {
        if (children.find(state) == children.end()) {
            children.emplace(state, SearchNode(state));
        }
        return children.find(state)->second;
    }

    VType sampleUtility(int depthLimit) {
        if (depthLimit == 0 || gameState.isTerminal()) {
            return gameState.estimateU();
        }
        Action a0 = sampleAction(Player::P0, regrets[0], totalPositiveRegret[0]);
        Action a1 = sampleAction(Player::P1, regrets[1], totalPositiveRegret[1]);

        VType result = getChild(gameState.next(a0, a1)).sampleUtility(depthLimit - 1);
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
        VType utility = getChild(nextState).visit(depthLimit - 1);

        vector<Action> samples0 = sampleRandomActions(Player::P0);
        vector<Action> samples1 = sampleRandomActions(Player::P1);

        for(auto aa0 : samples0) {
            addActionRegret(Player::P0, aa0,
                    getChild(gameState.next(aa0, a1)).sampleUtility(depthLimit) - utility);
        }
        for(auto aa1 : samples1) {
            // player 1's utility is negated
            addActionRegret(Player::P1, aa1,
                    -(getChild(gameState.next(a0, aa1)).sampleUtility(depthLimit) - utility));
        }
#ifdef DEBUG
        sampleUtilitySum += utility;
        sampleUtilityCnt++;
#endif
        return utility;
    }

    void dump(ostream& os, int depth) {
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
            for(auto it : actionFreq[player]) {
                os << indent << "    " << it.second << "(" << (Float)it.second / totalFreq[player]
                        << "): " << it.first.dumps() << endl;
            }
            os << indent << "  regrets of player " << player << ":" << endl;
            for(auto it : regrets[player]) {
                os << indent << "    " << it.second << ": " << it.first.dumps() << endl;
            }
        }
        os << indent << "  children:" << endl;
        for(auto it : children) {
            it.second.dump(os, depth + 1);
        }
        os << indent << "SearchNode ends" << endl;
    }

protected:
    // TODO will cumulative regret exceed int range?
    HashMap<Action, VType> regrets[N_PLAYER]; // from action id (distinct between both players) to cfr
    VType totalPositiveRegret[N_PLAYER];

    HashMap<Action, int> actionFreq[N_PLAYER]; // the frequency an action is played for our final sampling
    int totalFreq[N_PLAYER];

    State gameState;

    HashMap<State, SearchNode> children;

    // P0 for lowerV (P0 first)
    // P1 for upperV (P1 first)
    Float v(Player player) {
        if (gameState.isTerminal()) {
            return gameState.estimateU();
        }

        int sign = player == Player::P0 ? 1 : -1;
        Player otherPlayer = (Player)(1 - (int)player);

        if (actionFreq[(int)player].empty()) {
            for(auto action : sampleRandomActions(player))
                addActionFreq(player, action);
        }

        vector<Action> otherActions = sampleRandomActions(otherPlayer);
        HashMap<Action, Float> vMap;
        for(auto otherAction : otherActions)
            vMap[otherAction] = 0;
        for(auto it : actionFreq[(int)player]) {
            auto thisAction = it.first;
            Float p = (Float)it.second / totalFreq[(int)player];
            for(auto otherAction : otherActions) {
                auto a0 = player == Player::P0 ? thisAction : otherAction;
                auto a1 = player == Player::P0 ? otherAction : thisAction;
                vMap[otherAction] += p * getChild(gameState.next(a0, a1)).v(player);
            }
        }

        Float result = MAX_V;
        for(auto otherAction : otherActions)
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
