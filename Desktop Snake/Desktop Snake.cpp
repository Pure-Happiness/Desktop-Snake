#include "resource.h"
#include "targetver.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <ExDisp.h>
#include <ShlObj.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <optional>
#include <queue>
#include <vector>

static std::atomic_char dialog_result;

static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM) {
  switch (message) {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDOK:
      dialog_result = 'T';
      goto notify;
    case IDCANCEL:
      dialog_result = 'F';
      goto notify;
    }
  }
  return (INT_PTR)FALSE;
notify:
  dialog_result.notify_one();
  EndDialog(hDlg, LOWORD(wParam));
  return (INT_PTR)TRUE;
}

static bool WaitDialog(LPCWSTR lpTemplate) {
  dialog_result = 'N';
  DialogBox(nullptr, lpTemplate, nullptr, DialogProc);
  dialog_result.wait('N');
  return dialog_result == 'T';
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  {
    HANDLE hMutex = CreateMutex(nullptr, TRUE, L"DESKTOPSNAKE");
    assert(hMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
      return 0;
  }
  CoInitialize(nullptr);
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
  if (cItems < 10 && !WaitDialog(MAKEINTRESOURCE(IDD_FEWICONS)))
    return 0;
  DWORD dwFlags;
  assert(SUCCEEDED(pfv->GetCurrentFolderFlags(&dwFlags)));
  constexpr DWORD requiredFlags = FWF_AUTOARRANGE | FWF_SNAPTOGRID;
  assert(SUCCEEDED(pfv->SetCurrentFolderFlags(requiredFlags, 0)));
  POINT pt;
  assert(SUCCEEDED(pfv->GetSpacing(&pt)));
  PITEMID_CHILD *const apidl = new PITEMID_CHILD[cItems];
  auto set_icon = [pfv, apidl](int i, POINT p) {
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
  const POINT screen{.x = rect.right * (LONG)dpi / 96 / pt.x,
                     .y = rect.bottom * (LONG)dpi / 96 / pt.y};
  constexpr POINT dir[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
  const POINT DIR[]{{pt.x, 0}, {0, pt.y}, {-pt.x, 0}, {0, -pt.y}};
  struct Unit {
    LONG x, y, X, Y;
    int index;
  };
  std::srand(std::time(nullptr));
  while (true) {
    for (int i = 1; i < cItems; ++i)
      set_icon(i, {-pt.x, -pt.y});
    set_icon(0, {});
    std::size_t d = 0, banned = 2;
    std::vector<Unit> body{Unit{}};
    std::queue<Unit> food;
    std::vector<std::vector<bool>> occupied(screen.x,
                                            std::vector<bool>(screen.y));
    occupied[0][0] = true;
    int next_index = 1;
    std::optional<Unit> next_food;
    auto set_food = [&occupied, &next_index, &next_food, &set_icon, cItems, pt,
                     screen] {
      if (next_index < cItems) {
        LONG x = std::rand() % screen.x, y = std::rand() % screen.y;
        while (occupied[x][y])
          x = std::rand() % screen.x, y = std::rand() % screen.y;
        LONG X = x * pt.x, Y = y * pt.y;
        next_food = {x, y, X, Y, next_index};
        set_icon(next_index++, {.x = X, .y = Y});
      } else
        next_food = std::nullopt;
    };
    set_food();
    while (true) {
      Sleep(1000); // For debug. Decrease the time interval later.
      banned = d ^ 2;
      Unit old_tail = body.back();
      for (auto it = body.rbegin(), nit = next(it); nit != body.rend();
           it = nit++) {
        it->x = nit->x, it->y = nit->y;
        set_icon(it->index, {.x = it->X = nit->X, .y = it->Y = nit->Y});
      }
      if (!food.empty()) {
        auto &f = food.front();
        if (old_tail.x == f.x && old_tail.y == f.y) {
          body.push_back(f);
          food.pop();
          goto no_remove_end;
        }
      }
      occupied[old_tail.x][old_tail.y] = false;
    no_remove_end:
      Unit &head = body.front();
      if ((head.x += dir[d].x) >= 0 && head.x < screen.x &&
          (head.y += dir[d].y) >= 0 && head.y < screen.y)
        if (auto cell = occupied[head.x][head.y]; !cell) {
          cell = true;
          set_icon(head.index,
                   {.x = head.X += DIR[d].x, .y = head.Y += DIR[d].y});
          if (next_food)
            if (auto &nf = *next_food; head.x == nf.x && head.y == nf.y) {
              food.push(*next_food);
              set_food();
            }
          continue;
        }
      break;
    }
    if (!WaitDialog(MAKEINTRESOURCE(IDD_GAMEOVER)))
      break;
  }
  for (int i{}; i < cItems; ++i)
    set_icon(i, apt[i]);
  pfv->SetCurrentFolderFlags(requiredFlags, dwFlags);
  return 0;
}
