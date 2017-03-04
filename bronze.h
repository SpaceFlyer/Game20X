using namespace game20x;

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace std;

constexpr int MAX_PROD = 3;
constexpr int MAX_FACTORY = 15;
constexpr int MAX_LINK = 105;
constexpr int MOVE_BIT = 2;
constexpr int DEPTH = 5; // 10;
constexpr int SAMPLE_COUNT = 1; // 64;
constexpr int MAX_T = 100000000;
constexpr int TLE0 = 900; // ms
constexpr int TLE1 = 40; // ms
constexpr int PROD_THRESHOLD = 10;
constexpr int PROD_COST = 10;

constexpr int PROD_MULTIPLIER = 4;

auto start = clock();

int factoryCount;
int linkCount;
int turn = 0;

struct Link {
    int f1, f2, dist;
};

const char* FACTORY = "FACTORY";
const char* TROOP = "TROOP";

struct Factory {
    char side;
    Byte prod;
    short borgs;

    string dumps() const {
        sprintf(gCharBuffer, "[side=%d, prod=%d, borgs=%d]", side, prod, borgs);
        return gCharBuffer;
    }
};

struct Troop {
    char side;
    Byte from;
    Byte to;
    Byte eta;
    int num;

    string dumps() const {
        sprintf(gCharBuffer, "[side=%d, from=%d, to=%d, eta=%d, num=%d]", side, from, to, eta, num);
        return gCharBuffer;
    }
};

Link links[MAX_LINK];
int gDist[MAX_FACTORY][MAX_FACTORY];

template<typename T>
T abs(T a) { return a < 0 ? -a : a; }

class GhostState;

struct SingleMoveAction {
    static constexpr int BIT = MOVE_BIT;
    static constexpr int MAX_MOVE = 1 << BIT;;
    static constexpr int MOVE_CNT = (1 << (BIT + 1)) + 1;
    static constexpr int LINK_MULT = 1 << (BIT + 2);
    static constexpr int MAX_HASH = MAX_LINK * LINK_MULT + MOVE_CNT;

    int linkId;
    int move; // between - (1 << BIT) to + (1 << BIT)

    inline int hash() const {
        ASSERT(linkId >= 0);
        return move == 0 ? 0 : linkId * (1 << (BIT + 2)) + move + (1 << BIT);
    }

    inline bool operator==(const SingleMoveAction& other) const {
        return hash() == other.hash();
    }

    inline static int MoveBorgs(int move, int borgs1, int borgs2) {
        if (move < 0) {
            swap(borgs1, borgs2);
            move = -move;
        }
        if (move == MAX_MOVE)
            return borgs1;
        return borgs1 <= (1 << BIT) ? min(borgs1, move) : (borgs1 >> BIT) * move;
    }

    inline string dumps() const {
        return dumps(1 << BIT, 1 << BIT);
    }

    inline string dumps(const GhostState& state) const;

    inline string dumps(decltype(Factory::borgs) f1borgs, decltype(Factory::borgs) f2borgs) const {
        if (linkId < 0) {
            return "WAIT";
        }
        int f1 = links[linkId].f1;
        int f2 = links[linkId].f2;
        int moveBorgs = MoveBorgs(move, f1borgs, f2borgs);
        if (move < 0) {
            swap(f1, f2);
        }
        if (moveBorgs == 0)
            return "WAIT";
        return "MOVE " + to_string(f1) + " " + to_string(f2) + " " + to_string(moveBorgs);
    }
};

struct MetaAction {
    static constexpr int WAIT_CODE = -1;
    static constexpr int GREEDY_CODE = -2;
    static constexpr int PROD_CODE = -3;
    static constexpr int MAX_HASH = MAX_FACTORY * SingleMoveAction::MAX_MOVE + 3;

    int code;

    inline int hash() const { return code + 3; }
    bool operator==(const MetaAction& other) const { return code == other.code; }

    int target() const {
        ASSERT(code >= 0);
        return code / SingleMoveAction::MAX_MOVE;
    }

    int move() const {
        ASSERT(code >= 0);
        return code % SingleMoveAction::MAX_MOVE + 1; // 0 move is pointless
    }

    inline string dumps() const {
        if (code == WAIT_CODE)
            return "WAIT";
        else if (code == GREEDY_CODE)
            return "GREEDY";
        else if (code == PROD_CODE)
            return "PROD";
        else
            return "FOCUS " + to_string(target()) + " " + to_string(move());
    }
    inline string dumps(const GhostState& state) const;
};

constexpr int SingleMoveAction::MAX_MOVE;

struct GhostState {
    int turn;
    vector<Factory> factories;
    vector<Troop> troops;

    GhostState() {
        fEstimateU = MIN_V;
    }

    bool isTerminal() const {
        return turn > 200;
    }

    VType estimateU() {
        if (fEstimateU == MIN_V) {
            int multiplier = isTerminal() ? 1 : PROD_MULTIPLIER;
            fEstimateU = 0;
            for(const auto& f : factories) {
                fEstimateU += f.side * (f.borgs + f.prod * multiplier);
                // cerr << "EstimateU: " << fEstimateU << " f" << f.dumps() << endl;
            }
            for(const auto& t : troops) {
                fEstimateU += t.num * t.side;
                // cerr << "EstimateU: " << fEstimateU << " t" << t.dumps() << endl;
            }
        }
        // cerr << endl << endl;
        return fEstimateU;
    }

    Troop createTroop(Player player, const SingleMoveAction& a) {
        constexpr Troop noTroop = {0, 0, 0, 0, 0};
        if (a.linkId < 0) {
            return noTroop;
        }

        int playerSign = player == Player::P0 ? 1 : -1;
        Factory& f1 = factories[links[a.linkId].f1];
        Factory& f2 = factories[links[a.linkId].f2];
        Factory& ff = a.move > 0 ? f1 : f2;
        if (ff.side * playerSign != 1)
            return noTroop;

        int num = SingleMoveAction::MoveBorgs(a.move, f1.borgs, f2.borgs);
        auto from = decltype(Troop::from)(a.move > 0 ? links[a.linkId].f1 : links[a.linkId].f2);
        auto to = decltype(Troop::to)(links[a.linkId].f1 + links[a.linkId].f2 - from);
        auto eta = decltype(Troop::eta)(links[a.linkId].dist);
        ff.borgs -= num;
        ASSERT(ff.borgs >= 0);
        return {decltype(Troop::side)(playerSign), from, to, eta, num};
    }

    GhostState next(GhostState& nextState, const vector<SingleMoveAction>& a0s, const vector<SingleMoveAction>& a1s) const {
        // Move troops
        for(int i = 0; i < nextState.troops.size(); ++i) {
            --nextState.troops[i].eta;
        }

        // Excute orders
        for(const auto& a0 : a0s) {
            Troop t0 = nextState.createTroop(Player::P0, a0);
            if (t0.num)
                nextState.troops.push_back(t0);
        }
        for(const auto& a1 : a1s) {
            Troop t1 = nextState.createTroop(Player::P1, a1);
            if (t1.num)
                nextState.troops.push_back(t1);
        }

        // Produce new borgs
        for(auto& f : nextState.factories)
            if (f.side)
                f.borgs += f.prod;

        // Solve battles
        using VI = vector<int>;
        VI army[2] = {VI(factoryCount, 0), VI(factoryCount, 0)};
        for(int i = 0; i < nextState.troops.size(); ++i)
            if (nextState.troops[i].eta == 0) {
                army[nextState.troops[i].side == 1 ? 0 : 1][nextState.troops[i].to] +=
                        nextState.troops[i].num;
                swap(nextState.troops[i], nextState.troops[nextState.troops.size() - 1]);
                nextState.troops.pop_back();
                --i;
            }
        for(int i = 0; i < factoryCount; ++i) {
            int army0 = army[0][i] - army[1][i];
            Factory& f = nextState.factories[i];
            if (army0 != 0) {
                if (f.side == 1)
                    f.borgs += army0;
                else if (f.side == -1)
                    f.borgs -= army0;
                else
                    f.borgs -= abs(army0);

                if (f.borgs < 0) {
                    f.borgs = -f.borgs;
                    f.side = army0 > 0 ? 1 : -1;
                }
            }
        }

        return nextState;
    }

    void increaseProduction(GhostState& nextState, Player player) const {
        int playerSign = player == Player::P0 ? 1 : -1;
        for(auto& f : nextState.factories)
            if (f.side == playerSign && f.prod < MAX_PROD && f.borgs >= PROD_THRESHOLD) {
                f.borgs -= PROD_COST;
                f.prod++;
            }
    }

    vector<SingleMoveAction> genGreedyAs(Player player) const {
        vector<SingleMoveAction> result;
        int playerSign = player == Player::P0 ? 1 : -1;
        for(int linkId = 0; linkId < linkCount; ++linkId) {
            const auto& link = links[linkId];
            const auto* f1 = &factories[link.f1];
            const auto* f2 = &factories[link.f2];
            int moveSign = 1;
            if (f1->side != playerSign) {
                swap(f1, f2);
                moveSign = -1;
            }
            if (f1->side == playerSign && f2->side == 0 && f1->borgs > f2->borgs && f2->prod > 0) {
                int absMove = f1->borgs <= SingleMoveAction::MAX_MOVE
                        ? f2->borgs
                        : (int)ceil((Float)f2->borgs / f1->borgs * SingleMoveAction::MAX_MOVE);
                result.push_back({linkId, absMove * moveSign});
            }
        }
        return result;
    }

    vector<SingleMoveAction> genFocusAs(Player player, const MetaAction& a) const {
        vector<SingleMoveAction> result;
        int target = a.target();
        int absMove = a.move();
        for(int linkId = 0; linkId < linkCount; ++linkId) {
            int f1 = links[linkId].f1;
            int f2 = links[linkId].f2;
            if (gDist[f1][target] == -1 || gDist[f2][target] == -1)
                continue;
            if (gDist[f1][target] == links[linkId].dist + gDist[f2][target])
                result.push_back({linkId, absMove});
            if (gDist[f2][target] == links[linkId].dist + gDist[f1][target])
                result.push_back({linkId, -absMove});
        }
        return result;
    }

    inline vector<SingleMoveAction> genSingleAs(Player player, const MetaAction& a) const {
        if (a.code == MetaAction::GREEDY_CODE)
            return genGreedyAs((Player)player);
        else if (a.code >= 0) {
            return genFocusAs((Player)player, a);
        }
        return {};
    }

    GhostState next(const MetaAction& a0, const MetaAction& a1) const {
        GhostState nextState = *this;
        nextState.turn++;

        const MetaAction* metaAs[2] = {&a0, &a1};
        vector<SingleMoveAction> singleAs[2];

        for(int player = 0; player < N_PLAYER; ++player) {
            const MetaAction& metaA = *metaAs[player];
            if (metaA.code == MetaAction::PROD_CODE)
                increaseProduction(nextState, (Player)player);
            singleAs[player] = genSingleAs((Player)player, metaA);
        }

        return next(nextState, singleAs[0], singleAs[1]);
    }

    vector<MetaAction> getAllActions(Player player) {
        vector<MetaAction> result;
        for(int i = 0; i < factoryCount + 2; ++i)
            result.push_back({i - 2});
        return result;
    }

    int getAllActionCount(Player player) {
        return factoryCount * SingleMoveAction::MAX_MOVE + 3;
    }

    MetaAction sampleRandomAction(Player player) {
        return {rand() % (factoryCount * SingleMoveAction::MAX_MOVE + 3) - 3};
    }

    string dumps() const {
        string result = "turn " + to_string(turn);
        for(int i = 0; i < factories.size(); ++i) {
            result += " f" + to_string(i) + "=" + factories[i].dumps();
        }
        for(int i = 0; i < troops.size(); ++i) {
            result += " t" + to_string(i) + "=" + troops[i].dumps();
        }
        return result;
    }

private:
    int fEstimateU;
};

string SingleMoveAction::dumps(const GhostState& state) const {
    if (linkId < 0)
        return dumps();
    const Factory& f1 = state.factories[links[linkId].f1];
    const Factory& f2 = state.factories[links[linkId].f2];
    const Factory& ff = move > 0 ? f1 : f2;
    if (ff.side != 1)
        return "WAIT";
    return dumps(f1.borgs, f2.borgs);
}

string MetaAction::dumps(const GhostState& state) const {
    vector<SingleMoveAction> singleAs = state.genSingleAs(Player::P0, *this);
    string result = "WAIT";
    if (code == PROD_CODE)
        for(int i = 0; i < factoryCount; ++i) {
            const auto& f = state.factories[i];
            if (f.side == 1 && f.borgs >= PROD_THRESHOLD)
                result += "; INC " + to_string(i);
        }
    for(const auto& a : singleAs)
        result += ";" + a.dumps(state);
    return result;
}

using GhostSearchNode = SearchNode<MetaAction, GhostState, SAMPLE_COUNT>;

template <>
int GhostSearchNode::MemoryIndex = 0;

GhostSearchNode gMemory[MAX_NODE_COUNT];

template <>
GhostSearchNode* GhostSearchNode::Memory = gMemory;

/**
 * Auto-generated code below aims at helping you parse
 * the standard input according to the problem statement.
 **/
int main()
{
    cin >> factoryCount; cin.ignore();
    cin >> linkCount; cin.ignore();
    for (int i = 0; i < linkCount; i++) {
        cin >> links[i].f1 >> links[i].f2 >> links[i].dist; cin.ignore();
    }

    memset(gDist, -1, sizeof(gDist));
    for(int i = 0; i < factoryCount; ++i)
        gDist[i][i] = 0;
    for(const auto& link : links) {
        gDist[link.f1][link.f2] = link.dist;
        gDist[link.f2][link.f1] = link.dist;
    }
    for(int k = 0; k < factoryCount; ++k)
        for(int i = 0; i < factoryCount; ++i)
            for(int j = 0; j < factoryCount; ++j)
                if (gDist[i][k] > -1 && gDist[k][j] > -1)
                    if (gDist[i][j] == -1 || gDist[i][j] > gDist[i][k] + gDist[k][j])
                        gDist[i][j] = gDist[i][k] + gDist[k][j];

    // game loop
    while (1) {
        int clockLimit = CLOCKS_PER_SEC / 1000;
        clockLimit *= turn == 0 ? TLE0 : TLE1;
        cerr << "clockLimit: " << clockLimit << endl; // TODO TEST

        turn++;

        int entityCount; // the number of entities (e.g. factories and troops)
        cin >> entityCount; cin.ignore();

        int id;
        string type;
        int args[5];

        GhostState initialState;
        initialState.turn = turn;

        for (int i = 0; i < entityCount; i++) {
            int id;
            string type;
            cin >> id >> type;
            for(int j = 0; j < 5; ++j)
                cin >> args[j];
            if (type == FACTORY) {
                ASSERT(id < factoryCount);
                initialState.factories.push_back({
                    (decltype(Factory::side))args[0],
                    (decltype(Factory::prod))args[2],
                    (decltype(Factory::borgs))args[1]
                });
            } else if (type == TROOP) {
                initialState.troops.push_back({
                    (decltype(Troop::side))args[0],
                    (decltype(Troop::from))args[1],
                    (decltype(Troop::to))args[2],
                    (decltype(Troop::eta))args[4],
                    (decltype(Troop::num))args[3],
                });
            }
            cin.ignore();
        }

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;

        GhostSearchNode::ClearMemory();
        GhostSearchNode& root = *GhostSearchNode::Make(initialState);
        int t = 0;
        while (true && t < MAX_T) {
            auto now = clock();
            if (now - start >= clockLimit)
                break;
            t++;
            root.visit(DEPTH);
        }

        MetaAction action = root.sampleFinalAction(Player::P0);

        cerr << "t: " << t << " time: " << (double)(clock() - start) / CLOCKS_PER_SEC << endl;
        cerr << action.dumps() << endl;
        root.dump(cerr, 0, 0); // TODO TEST

        // Any valid action, such as "WAIT" or "MOVE source destination cyborgs"
        cout << action.dumps(initialState) << endl;

        start = clock();
    }
}
