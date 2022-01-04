#include <stdio.h>
#include <Windows.h>
#include <conio.h>
#include "lodepng.h"

DWORD read_remote_dword(HANDLE proc, DWORD addr) {
    DWORD ret = 0;
    if (!ReadProcessMemory(proc, (LPCVOID)addr, &ret, sizeof(ret), NULL)) {
        printf("ReadProcessMemory at 0x%p failed!\n", (LPCVOID)addr);
        ExitProcess(1);
    }
    return ret;
}

BYTE read_board(HANDLE proc, DWORD x, DWORD y) {
    BYTE ret = 0;
    if (!ReadProcessMemory(proc, (LPCVOID)(0x01005340 + 32 * y + x), &ret, sizeof(ret), NULL)) {
        printf("ReadProcessMemory failed!\n");
        ExitProcess(1);
    }
    return ret;
}

void write_board(HANDLE proc, DWORD x, DWORD y, BYTE val) {
    if (!WriteProcessMemory(proc, (LPVOID)(0x01005340 + 32 * y + x), &val, 1, NULL)) {
        printf("WriteProcessMemory failed!\n");
        ExitProcess(1);
    }
}

void board_xray(HANDLE proc) {
    while (true) {
        DWORD board_width = read_remote_dword(proc, 0x01005334);
        DWORD board_height = read_remote_dword(proc, 0x01005338);
        printf("board width: %u, board height: %u\n", board_width, board_height);
        for (int y = 1; y < board_height + 1; y++) {
            for (int x = 1; x < board_width + 1; x++) {
                printf("%02X ", read_board(proc, x, y));
                //printf("%s ", (read_board(proc, x, y) & 0x80) ? "X" : (read_board(proc, x, y) & 0x40) ? " " : "*");
            }
            printf("\n");
        }
        printf("\n");
        Sleep(1000);
    }
}

void board_badapple(HANDLE proc, HWND window) {
    // board has max dimensions of 24x30
    DWORD board_width = read_remote_dword(proc, 0x01005334);
    DWORD board_height = read_remote_dword(proc, 0x01005338);
    if (board_width != 30 || board_height != 24) {
        printf("wrong board dimensions! please start a new game with a height of 24 and a width of 30\n");
        return;
    }

    // would probably be much smarter to preprocess these frames and embed them in the binary instead
    printf("loading frames, please wait warmly\n");
    std::vector<std::vector<unsigned char>> frames;
    for (int i = 1; ; i++) {
        std::vector<unsigned char> data;
        unsigned img_w = 0;
        unsigned img_h = 0;
        char path[MAX_PATH] = {};
        snprintf(path, sizeof(path), "C:\\Users\\Owner\\Downloads\\badappleframes\\frame_%d.png", i);
        if (lodepng::decode(data, img_w, img_h, path))
            break;
        if (img_w != 30 || img_h != 24) {
            printf("frame %d is the wrong size\n", i);
            return;
        }
        frames.push_back(data);
    }
    if (frames.size() == 0) {
        printf("no frames were loaded\n");
        return;
    }
    printf("loaded %u frames! press enter to start\n", frames.size());
    _getch();

    // playback loop
    // this is quite inefficient, it would be much smarter to store frame diffs
    // or just rewrite the whole board with a single WriteProcessMemory call
    timeBeginPeriod(1);
    constexpr double FRAMERATE = 30.0;
    unsigned cur_frame = 0;
    unsigned last_frame = 0;
    DWORD start_time = timeGetTime();

    while (true) {
        // get the next frame based on the current time compared to the start
        // if time hasn't advanced enough, keep checking until it advances enough
        cur_frame = (int)((double)(timeGetTime() - start_time) / (1000.0 / FRAMERATE));
        if (cur_frame == last_frame)
            continue;
        if (cur_frame >= frames.size())
            break;
        last_frame = cur_frame;

        // write the frame to the board
        for (int y = 0; y < 24; y++) {
            for (int x = 0; x < 30; x++) {
                bool filled = frames[cur_frame][(x + y * 30) * 4] < 127;
                // 0x8A = exposed bomb
                // 0x40 = blank clicked spot
                // 0x0F = unclicked spot
                // i think this color scheme looks the nicest
                write_board(proc, x + 1, y + 1, filled ? 0x8A : 0x40);
            }
        }

        // force the window to refresh
        RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }
    
    timeEndPeriod(1);
}

int main() {
    // find minesweeper's window
    HWND window = FindWindowA(NULL, "Minesweeper");
    if (window == NULL) {
        printf("failed to find minesweeper window! gle: 0x%x\n", GetLastError());
        return 1;
    }
    printf("got window: 0x%p\n", window);

    // get minesweeper's pid
    DWORD pid = NULL;
    GetWindowThreadProcessId(window, &pid);
    printf("got pid: 0x%x\n", pid);

    // open minesweeper's process with full rw access
    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
    if (proc == NULL) {
        printf("failed to open minesweeper process! gle: 0x%x\n", GetLastError());
        return 1;
    }
    printf("got process handle: 0x%p\n", proc);

    board_badapple(proc, window);
    //board_xray(proc);

    CloseHandle(proc);
    return 0;
}