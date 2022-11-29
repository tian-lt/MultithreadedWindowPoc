## PoC usage

1. Build and launch via a debug tool, i.e. Visual Studio.
1. You will two windows are being launched. One has a rectangle with black edges, and the other is blank.
1. Click on `Menu > Help > About` to update test-state form 0 to 1. This action will send a message to the unique child window (which has the rectangle with black edges), and make the corresponding window procedure to print debug information into the *Output* window. e.g., `tag = childwnd_proc[0], tid = 76560, hparent = 14550858 hwnd = 35718854, msg = 2025`.
1. Click on the `About` option again. The test state will transfer from 1 to 2. The app will move the child window to the blank window. Similarly, the *Output* window will print debug messages.
1. Click on the `About` option. The test state will transfer from 2 to 3. The app will send a test message to the child window. Check out the *Output* window to see more information.
1. The test state will keep same once it has reached `state 3`.

## Screenshots

![initial state](/imgs/Screenshot_20221129_101845.png)

![outputs](/imgs/Screenshot_20221129_101951.png)

![child window moved](/imgs/Screenshot_20221129_102009.png)

![outputs](/imgs/Screenshot_20221129_102024.png)
