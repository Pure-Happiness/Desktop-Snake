#pragma warning(disable : 4700)
#include "Resource.h"
#include "targetver.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <ExDisp.h>
#include <ShlObj.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <queue>
#include <vector>

static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM) {
  switch (message) {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;
  case WM_COMMAND:
    if (auto lwParam = LOWORD(wParam); lwParam == IDOK || lwParam == IDCANCEL) {
      assert(EndDialog(hDlg, lwParam));
      return (INT_PTR)TRUE;
    }
  }
  return (INT_PTR)FALSE;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  {
    HANDLE hMutex = CreateMutex(nullptr, TRUE, L"DESKTOPSNAKE");
    assert(hMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
      return 0;
  }
  assert(SUCCEEDED(CoInitialize(nullptr)));
  IShellWindows *psw;
  assert(SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL,
                                    IID_PPV_ARGS(&psw))));
  IDispatch *pdisp;
  {
    VARIANT varLoc{VT_EMPTY};
    long temp;
    assert(psw->FindWindowSW(&varLoc, &varLoc, SWC_DESKTOP, &temp,
                             SWFO_NEEDDISPATCH, &pdisp) == S_OK);
  }
  IShellBrowser *psb;
  assert(SUCCEEDED(
      IUnknown_QueryService(pdisp, SID_STopLevelBrowser, IID_PPV_ARGS(&psb))));
  IShellView *psv;
  assert(SUCCEEDED(psb->QueryActiveShellView(&psv)));
  IFolderView2 *pfv;
  assert(SUCCEEDED(psv->QueryInterface(IID_PPV_ARGS(&pfv))));
  int cItems;
  assert(SUCCEEDED(pfv->ItemCount(SVGIO_ALLVIEW, &cItems)));
  if (cItems <= 0) {
    WCHAR message[64];
    assert(LoadString(hInstance, IDS_NO_ICON, message, 64) > 0);
    MessageBox(nullptr, message, nullptr, MB_OK);
    return 0;
  }
  if (cItems < 10 && DialogBox(nullptr, MAKEINTRESOURCE(IDD_FEWICONS), nullptr,
                               DialogProc) == IDCANCEL)
    return 0;
  DWORD dwFlags;
  assert(SUCCEEDED(pfv->GetCurrentFolderFlags(&dwFlags)));
  constexpr DWORD requiredFlags = FWF_AUTOARRANGE | FWF_SNAPTOGRID;
  assert(SUCCEEDED(pfv->SetCurrentFolderFlags(requiredFlags, 0)));
  POINT pt;
  assert(SUCCEEDED(pfv->GetSpacing(&pt)));
  PITEMID_CHILD *const apidl = new PITEMID_CHILD[cItems];
  auto sIcon = [pfv, apidl](int i, POINT p) {
    assert(SUCCEEDED(pfv->SelectAndPositionItems(1, (LPCITEMIDLIST *)apidl + i,
                                                 &p, SVSI_POSITIONITEM)));
  };
  POINT *const apt = new POINT[cItems];
  for (int i{}; i < cItems; ++i) {
    assert(SUCCEEDED(pfv->Item(i, apidl + i)));
    assert(SUCCEEDED(pfv->GetItemPosition(apidl[i], apt + i)));
  }
  HWND hWnd;
  assert(SUCCEEDED(psv->GetWindow(&hWnd)));
  RECT rect;
  assert(GetClientRect(hWnd, &rect));
  const UINT dpi = GetDpiForWindow(hWnd);
  const POINT SCREEN{rect.right * (LONG)dpi / 96, rect.bottom * (LONG)dpi / 96},
      screen{SCREEN.x / pt.x, SCREEN.y / pt.y};
  pt.x = SCREEN.x / screen.x, pt.y = SCREEN.y / screen.y;
  constexpr POINT dir[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
  const POINT DIR[]{{pt.x, 0}, {0, pt.y}, {-pt.x, 0}, {0, -pt.y}};
  constexpr int vKey[] = {VK_RIGHT, VK_DOWN, VK_LEFT, VK_UP},
                vKey2[] = {'D', 'S', 'A', 'W'};
  struct Unit {
    LONG x, y, X, Y;
    int index;
  };
  std::srand(std::time(nullptr));
  while (true) {
    for (int i = 1; i < cItems; ++i)
      sIcon(i, {-pt.x, -pt.y});
    sIcon(0, {});
    std::size_t d = 0, banned = 2;
    std::vector<Unit> body{Unit{}};
    std::queue<Unit> food;
    std::vector<std::vector<bool>> oc(screen.x, std::vector<bool>(screen.y));
    oc[0][0] = true;
    int nIndex = 1;
    std::optional<Unit> nFood;
    auto set_food = [&oc, &nIndex, &nFood, &sIcon, cItems, pt, screen] {
      if (nIndex < cItems) {
        LONG x = std::rand() % screen.x, y = std::rand() % screen.y;
        while (oc[x][y])
          x = std::rand() % screen.x, y = std::rand() % screen.y;
        LONG X = x * pt.x, Y = y * pt.y;
        nFood = {x, y, X, Y, nIndex};
        sIcon(nIndex++, {X, Y});
      } else
        nFood = std::nullopt;
    };
    set_food();
    while (true) {
      for (int i{}; i < 16; ++i) {
        Sleep(16);
        for (int i{}; i < 4; ++i)
          if (GetAsyncKeyState(vKey[i]) < 0 || GetAsyncKeyState(vKey2[i]) < 0) {
            if (i != banned)
              d = i;
            break;
          }
      }
      assert(SUCCEEDED(pfv->SetRedraw(FALSE)));
      banned = d ^ 2;
      Unit tail = body.back();
      for (auto it = body.rbegin(), nit = next(it); nit != body.rend();
           it = nit++) {
        it->x = nit->x, it->y = nit->y;
        sIcon(it->index, {it->X = nit->X, it->Y = nit->Y});
      }
      if (!food.empty()) {
        auto &f = food.front();
        if (tail.x == f.x && tail.y == f.y) {
          body.push_back(f);
          food.pop();
          goto no_remove_end;
        }
      }
      oc[tail.x][tail.y] = false;
    no_remove_end:
      Unit &head = body.front();
      sIcon(head.index, {head.X += DIR[d].x, head.Y += DIR[d].y});
      if ((head.x += dir[d].x) >= 0 && head.x < screen.x &&
          (head.y += dir[d].y) >= 0 && head.y < screen.y)
        if (auto cell = oc[head.x][head.y]; !cell) {
          cell = true;
          if (nFood)
            if (auto &nf = *nFood; head.x == nf.x && head.y == nf.y) {
              food.push(*nFood);
              set_food();
            }
          assert(SUCCEEDED(pfv->SetRedraw(TRUE)));
          continue;
        }
      break;
    }
    if (DialogBox(nullptr, MAKEINTRESOURCE(IDD_GAMEOVER), nullptr,
                  DialogProc) == IDCANCEL)
      break;
  }
  for (int i{}; i < cItems; ++i)
    sIcon(i, apt[i]);
  assert(SUCCEEDED(pfv->SetCurrentFolderFlags(requiredFlags, dwFlags)));
  return 0;
}
