#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>

#include <algorithm>
#include <memory>
#include <string>

namespace
{
	constexpr wchar_t windowClassName[] = L"AdnViewerWindow";

	std::unique_ptr<Gdiplus::Image> image;

	void drawImage(HDC deviceContext, const RECT& clientArea)
	{
		const int clientWidth = clientArea.right - clientArea.left;
		const int clientHeight = clientArea.bottom - clientArea.top;
		FillRect(deviceContext, &clientArea, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

		if (!image || clientWidth <= 0 || clientHeight <= 0)
		{
			return;
		}

		const UINT imageWidth = image->GetWidth();
		const UINT imageHeight = image->GetHeight();
		if (imageWidth == 0 || imageHeight == 0)
		{
			return;
		}

		const double scale = std::min(
			static_cast<double>(clientWidth) / imageWidth,
			static_cast<double>(clientHeight) / imageHeight);
		const int destinationWidth = static_cast<int>(imageWidth * scale);
		const int destinationHeight = static_cast<int>(imageHeight * scale);
		const int destinationX = (clientWidth - destinationWidth) / 2;
		const int destinationY = (clientHeight - destinationHeight) / 2;

		Gdiplus::Graphics graphics(deviceContext);
		graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
		graphics.DrawImage(image.get(), destinationX, destinationY, destinationWidth, destinationHeight);
	}

	LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_ERASEBKGND:
			return 1;

		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE)
			{
				DestroyWindow(window);
				return 0;
			}
			break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint = {};
			HDC deviceContext = BeginPaint(window, &paint);
			RECT clientArea = {};
			GetClientRect(window, &clientArea);
			drawImage(deviceContext, clientArea);
			EndPaint(window, &paint);
			return 0;
		}

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(window, message, wParam, lParam);
	}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
	Gdiplus::GdiplusStartupInput startupInput = {};
	ULONG_PTR gdiplusToken = 0;
	if (Gdiplus::GdiplusStartup(&gdiplusToken, &startupInput, nullptr) != Gdiplus::Ok)
	{
		return 1;
	}

	int argumentCount = 0;
	LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (!arguments || argumentCount < 2)
	{
		MessageBoxW(nullptr, L"No se recibio una imagen para mostrar.", L"ADN Viewer", MB_OK | MB_ICONERROR);
		if (arguments)
		{
			LocalFree(arguments);
		}
		Gdiplus::GdiplusShutdown(gdiplusToken);
		return 1;
	}

	const std::wstring imagePath = arguments[1];
	LocalFree(arguments);
	image = std::make_unique<Gdiplus::Image>(imagePath.c_str());
	if (image->GetLastStatus() != Gdiplus::Ok)
	{
		MessageBoxW(nullptr, L"No se pudo cargar la imagen seleccionada.", L"ADN Viewer", MB_OK | MB_ICONERROR);
		image.reset();
		Gdiplus::GdiplusShutdown(gdiplusToken);
		return 1;
	}

	WNDCLASSW windowClass = {};
	windowClass.hInstance = instance;
	windowClass.lpszClassName = windowClassName;
	windowClass.lpfnWndProc = windowProcedure;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	if (!RegisterClassW(&windowClass))
	{
		image.reset();
		Gdiplus::GdiplusShutdown(gdiplusToken);
		return 1;
	}

	const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	HWND window = CreateWindowExW(
		0,
		windowClassName,
		L"ADN Viewer",
		WS_POPUP,
		0,
		0,
		screenWidth,
		screenHeight,
		nullptr,
		nullptr,
		instance,
		nullptr);
	if (!window)
	{
		image.reset();
		Gdiplus::GdiplusShutdown(gdiplusToken);
		return 1;
	}

	SetWindowPos(window, HWND_TOPMOST, 0, 0, screenWidth, screenHeight, SWP_SHOWWINDOW);
	SetForegroundWindow(window);

	MSG message = {};
	while (GetMessageW(&message, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	image.reset();
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return static_cast<int>(message.wParam);
}