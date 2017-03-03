#include "game.h"

#include <ctime>

using namespace game20x;

VType data[16]; // 4 2x2 games

struct X2Action {
    int a;
    int hash() const { return a; }
    string dumps() const { return to_string(a); }

    X2Action(int ta = 0) : a(ta) {}

    bool operator== (const X2Action& other) const { return a == other.a; }
};

struct X2State {
    int d;
    int a0[2], a1[2];

    int hash() const {
        return d * 10000 + a0[0] * 1000 + a0[1] * 100 + a1[0] * 10 + a1[1] * 1;
    }

    bool operator== (const X2State& other) const { return hash() == other.hash(); }

    bool isTerminal() const { return d == 2; }
    VType estimateU() const {
        if (d != 2) {
            return 0;
        }
        // a0[0] a1[0] a0[1] a1[1]
        int index = a0[0] * 8 + a1[0] * 4 + a0[1] * 2 + a1[1];
        return data[index];
    }

    X2State next(const X2Action& a0, const X2Action& a1) const {
        auto xa0 = static_cast<const X2Action&>(a0);
        auto xa1 = static_cast<const X2Action&>(a1);
        X2State result = *this;
        result.d++;
        result.a0[d] = xa0.a;
        result.a1[d] = xa1.a;
        return result;
    }

    int getAllActionCount(Player player) const { return 2; }
    vector<X2Action> getAllActions(Player player) const {
        return {X2Action(0), X2Action(1)};
    }
    X2Action sampleRandomAction(Player player) const { return X2Action(rand() % 2); }


    string dumps() const {
        sprintf(gCharBuffer, "d = %d, a0 = %d, %d, a1 = %d, %d",
                d, a0[0], a0[1], a1[0], a1[1]);
        return gCharBuffer;
    }
};

using X2SearchNode = SearchNode<X2Action, X2State>;

int main() {
    srand(time(0));
    for(int i = 0; i < 16; ++i) {
        data[i] = rand() % 17 - 8;
        if (i >= 4) {
            // data[i] = data[i - 4]; // TODO TEST normal form
        }
        cout << data[i] << ' ';
        if (i % 2 == 1)
            cout << endl;
        if (i % 4 == 3)
            cout << endl;
    }
    cout << "data ended" << endl;

    X2State initialState;
    initialState.d = 0;
    X2SearchNode root(initialState);

    for(int t = 0; t < 10000; ++t) {
        root.visit(4); // depth limit is higher than terminal state depth
    }

    root.dump(cout, 0);

    return 0;
}
