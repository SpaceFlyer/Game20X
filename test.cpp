#define DEBUG

#include "game.h"

#include <ctime>

using namespace game20x;

#include "xnd.h"

// using X2Action = XNAction<2>;
// using X2State = XNDState<2, 2>;
// using X2SearchNode = SearchNode<X2Action, X2State>;
// using XState = X2State;
// using XSearchNode = X2SearchNode;

// using X3Action = XNAction<3>;
// using X32State = XNDState<3, 2>;
// using X3SearchNode = SearchNode<X3Action, X32State>;
// using XState = X32State;
// using XSearchNode = X3SearchNode;

// using X10Action = XNAction<10>;
// using X10State = XNDState<10, 1>;
// using XState = X10State;
// using XSearchNode = SearchNode<X10Action, X10State>;

using X25Action = XNAction<25>;
using X25State = XNDState<25, 1>;
using XState = X25State;
using XSearchNode = SearchNode<X25Action, X25State>;

template <>
int XSearchNode::MemoryIndex = 0;

XSearchNode gMemory[MAX_NODE_COUNT];

template <>
XSearchNode* XSearchNode::Memory = gMemory;

int main() {
    srand(0);
    // for(int i = 0; i < 16; ++i) {
    //     data[i] = rand() % 17 - 8;
    //     if (i >= 4) {
    //         data[i] = data[i - 4]; // TODO TEST normal form
    //     }
    //     cout << data[i] << ' ';
    //     if (i % 2 == 1)
    //         cout << endl;
    //     if (i % 4 == 3)
    //         cout << endl;
    // }
    // cout << "data ended" << endl;

    for(int i = 0; i < DATA_LEN; ++i) {
        data[i] = rand() % 17 - 8;
    }

    int n = 23;

    for(int r = 0; r < n; ++r)
        for(int c = 0; c < n; ++c)
            XState::SetSingleData(r, c, data[r * 25 + c]);

    XState::DumpFormattedData();

    XState initialState;
    initialState.setN(n);
    initialState.d = 0;
    XSearchNode::ClearMemory();
    XSearchNode& root = *XSearchNode::Make(initialState);

    for(int t = 0; t < 2000; ++t) {
        root.visit(4); // depth limit is higher than terminal state depth
    }

    root.dump(cout, 0);

    return 0;
}
