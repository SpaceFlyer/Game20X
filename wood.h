using namespace game20x;

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;

using uint16 = unsigned short;

constexpr int MAX_LINK = 105;
constexpr int MOVE_BIT = 3;
constexpr int DEPTH = 3; // 10;
constexpr int SAMPLE_COUNT = 5; // 64;
constexpr int MAX_T = 1; // 10000;

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
    uint16 borgs;

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

template<typename T>
T abs(T a) { return a < 0 ? -a : a; }

class GhostState;

struct SingleMoveAction {
    static constexpr int BIT = MOVE_BIT;
    static constexpr int MAX_MOVE = 1 << BIT;
    static constexpr int MOVE_CNT = (1 << (BIT + 1)) + 1;
    static constexpr int LINK_MULT = 1 << (BIT + 2);
    static constexpr int MAX_HASH = MAX_LINK * LINK_MULT + MOVE_CNT;

    int linkId;
    int move; // between - (1 << BIT) to + (1 << BIT)

    inline int hash() const {
        // -1 for wait
        return linkId < 0 ? -1 : linkId * (1 << (BIT + 2)) + move + (1 << BIT);
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

struct GhostState {
    static constexpr int PROD_MULTIPLIER = 4;

    int turn;
    vector<Factory> factories;
    vector<Troop> troops;

    GhostState() {
        fEstimateU = -1;
    }

    bool isTerminal() const {
        return turn > 200;
    }

    VType estimateU() {
        if (fEstimateU == -1) {
            int multiplier = isTerminal() ? 1 : PROD_MULTIPLIER;
            fEstimateU = 0;
            for(const auto& f : factories) {
                fEstimateU += f.side * (f.borgs + f.prod * multiplier);
            }
            for(const auto& t : troops) {
                fEstimateU += t.num * t.side;
            }
        }
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

    GhostState next(const SingleMoveAction& a0, const SingleMoveAction& a1) const {
        GhostState nextState = *this;
        nextState.turn++;

        // Move troops
        for(int i = 0; i < nextState.troops.size(); ++i) {
            --nextState.troops[i].eta;
        }

        // Excute orders
        Troop t0 = nextState.createTroop(Player::P0, a0);
        if (t0.num)
            nextState.troops.push_back(t0);
        Troop t1 = nextState.createTroop(Player::P1, a1);
        if (t1.num)
            nextState.troops.push_back(t1);

        // Produce new borgs
        for(auto& f : nextState.factories)
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

    int getAllActionCount(Player player) const {
        return linkCount * SingleMoveAction::MOVE_CNT + 1;
    }
    vector<SingleMoveAction> getAllActions(Player player) const {
        vector<SingleMoveAction> result;
        int playerSign = player == Player::P0 ? 1 : -1;
        for(int i = 0; i < linkCount; ++i) {
            const Factory& f1 = factories[links[i].f1];
            const Factory& f2 = factories[links[i].f2];
            if (f1.side * playerSign > 0) {
                for(int m = 1; m <= SingleMoveAction::MAX_MOVE; ++m)
                    result.push_back({i, m});
            }
            if (f2.side * playerSign > 0) {
                for(int m = 1; m <= SingleMoveAction::MAX_MOVE; ++m)
                    result.push_back({i, -m});
            }
        }
        result.push_back({-1, 0}); // WAIT
        return result;
    }

    SingleMoveAction sampleRandomAction(Player player) const {
        if (rand() % (SingleMoveAction::MOVE_CNT * linkCount) == 0)
            return {-1, 0}; // WAIT with low probability
        int m = (rand() % SingleMoveAction::MAX_MOVE + 1) * (2 * (rand() % 2) - 1);
        int linkId = rand() % linkCount;
        return {linkId, m};
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

using GhostSearchNode = SearchNode<SingleMoveAction, GhostState, SAMPLE_COUNT>;

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

    // game loop
    while (1) {
        auto start = clock();
        int clockLimit = CLOCKS_PER_SEC / 1000;
        clockLimit *= turn == 0 ? 20 : 20;

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
            } else {
                ASSERT(type == TROOP);
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

        GhostSearchNode root(initialState);
        int t = 0;
        while (true) {
            auto now = clock();
            if (now - start >= clockLimit)
                break;
            t++;
            root.visit(DEPTH);
        }

        SingleMoveAction action = root.sampleFinalAction(Player::P0);

        // Any valid action, such as "WAIT" or "MOVE source destination cyborgs"
        cout << action.dumps(initialState) << endl;

        cerr << "t: " << t << endl;
    }
}
