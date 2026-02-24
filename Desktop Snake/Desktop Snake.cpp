#include "resource.h"
#include "targetver.h"
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <ExDisp.h>
#include <ShlObj.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef NDEBUG
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

static bool operator==(const POINT &a, const POINT &b) {
  return a.x == b.x && a.y == b.y;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  {
    HANDLE hMutex = CreateMutex(nullptr, TRUE, L"DESKTOPSNAKE");
    assert(hMutex);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
      return 0;
  }
  assert(SUCCEEDED(CoInitialize(nullptr)));
  {
    IShellDispatch4 *psd;
    assert(SUCCEEDED(CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_SERVER,
                                      IID_PPV_ARGS(&psd))));
    assert(SUCCEEDED(psd->ToggleDesktop()));
  }
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
  HWND hWnd;
  assert(SUCCEEDED(psv->GetWindow(&hWnd)));
  int cItems;
  assert(SUCCEEDED(pfv->ItemCount(SVGIO_ALLVIEW, &cItems)));
  if (cItems <= 0) {
    DialogBox(hInstance, MAKEINTRESOURCE(IDD_NOICON), hWnd, nullptr);
    return 0;
  }
  if (cItems < 10 && DialogBox(hInstance, MAKEINTRESOURCE(IDD_FEWICONS), hWnd,
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
  RECT rect;
  assert(GetClientRect(hWnd, &rect));
  const UINT dpi = GetDpiForWindow(hWnd);
  const POINT SCREEN{rect.right * (LONG)dpi / 96, rect.bottom * (LONG)dpi / 96},
      screen{SCREEN.x / pt.x, SCREEN.y / pt.y},
      base{apt->x % pt.x, apt->y % pt.y};
  constexpr POINT dir[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
  const POINT DIR[]{{pt.x, 0}, {0, pt.y}, {-pt.x, 0}, {0, -pt.y}};
  constexpr int vKey[] = {VK_RIGHT, VK_DOWN, VK_LEFT, VK_UP};
  struct Unit {
    POINT l, p;
    int index;
  };
  std::srand(std::time(nullptr));
  while (true) {
    for (int i = 1; i < cItems; ++i)
      sIcon(i, {-pt.x, -pt.y});
    sIcon(0, base);
    std::size_t d = 0, banned = 2;
    std::vector<Unit> body{{{}, base, 0}};
    std::queue<Unit> food;
    std::vector<std::vector<bool>> oc(screen.x, std::vector<bool>(screen.y));
    oc[0][0] = true;
    int nIndex = 1;
    std::optional<Unit> nFood;
    auto set_food = [&oc, &nIndex, &nFood, &sIcon, cItems, pt, screen, base] {
      if (nIndex < cItems) {
        POINT logic{std::rand() % screen.x, std::rand() % screen.y};
        while (oc[logic.x][logic.y])
          logic.x = std::rand() % screen.x, logic.y = std::rand() % screen.y;
        POINT physic{logic.x * pt.x + base.x, logic.y * pt.y + base.y};
        nFood = {logic, physic, nIndex};
        sIcon(nIndex++, physic);
      } else
        nFood = std::nullopt;
    };
    set_food();
    while (true) {
      for (int i{}; i < 15; ++i) {
        Sleep(6);
        for (int i{}; i < 4; ++i)
          if (GetAsyncKeyState(vKey[i]) < 0) {
            if (i != banned)
              d = i;
            break;
          }
      }
      assert(SUCCEEDED(pfv->SetRedraw(FALSE)));
      banned = d ^ 2;
      POINT tail = body.back().l;
      for (auto it = body.rbegin(), nit = next(it); nit != body.rend();
           it = nit++) {
        it->l = nit->l;
        sIcon(it->index, it->p = nit->p);
      }
      if (!food.empty()) {
        Unit &f = food.front();
        if (tail == f.l) {
          body.push_back(f);
          food.pop();
          goto no_remove_end;
        }
      }
      oc[tail.x][tail.y] = false;
    no_remove_end:
      Unit &head = body.front();
      sIcon(head.index, {head.p.x += DIR[d].x, head.p.y += DIR[d].y});
      if ((head.l.x += dir[d].x) >= 0 && head.l.x < screen.x &&
          (head.l.y += dir[d].y) >= 0 && head.l.y < screen.y)
        if (auto cell = oc[head.l.x][head.l.y]; !cell) {
          cell = true;
          if (nFood)
            if (Unit &nf = *nFood; head.l == nf.l) {
              food.push(nf);
              set_food();
            }
          assert(SUCCEEDED(pfv->SetRedraw(TRUE)));
          continue;
        }
      break;
    }
    if (DialogBox(hInstance, MAKEINTRESOURCE(IDD_GAMEOVER), hWnd, DialogProc) ==
        IDCANCEL)
      break;
  }
  for (int i{}; i < cItems; ++i)
    sIcon(i, apt[i]);
  assert(SUCCEEDED(pfv->SetCurrentFolderFlags(requiredFlags, dwFlags)));
  return 0;
}
