#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <gdiplus.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
	constexpr wchar_t windowClassName[] = L"AdnViewerWindow";

	struct ImageEntry
	{
		std::wstring path;
		FILETIME lastWriteTime;
	};

	std::unique_ptr<Gdiplus::Image> image;
	std::vector<ImageEntry> images;
	std::size_t currentImageIndex = 0;

	std::vector<ImageEntry> findImages(const std::wstring& selectedImagePath)
	{
		const std::wstring::size_type separator = selectedImagePath.find_last_of(L"\\/");
		if (separator == std::wstring::npos)
		{
			return {};
		}

		const std::wstring directory = selectedImagePath.substr(0, separator);
		const std::wstring searchPattern = directory + L"\\*.png";
		WIN32_FIND_DATAW findData = {};
		HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
		if (findHandle == INVALID_HANDLE_VALUE)
		{
			return {};
		}

		std::vector<ImageEntry> foundImages;
		do
		{
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				foundImages.push_back({directory + L"\\" + findData.cFileName, findData.ftLastWriteTime});
			}
		} while (FindNextFileW(findHandle, &findData));
		FindClose(findHandle);

		std::sort(foundImages.begin(), foundImages.end(), [](const ImageEntry& left, const ImageEntry& right)
		{
			const int timeComparison = CompareFileTime(&left.lastWriteTime, &right.lastWriteTime);
			if (timeComparison != 0)
			{
				return timeComparison > 0;
			}

			return lstrcmpiW(left.path.c_str(), right.path.c_str()) < 0;
		});
		return foundImages;
	}

	bool loadImage(std::size_t index)
	{
		std::unique_ptr<Gdiplus::Image> nextImage = std::make_unique<Gdiplus::Image>(images[index].path.c_str());
		if (nextImage->GetLastStatus() != Gdiplus::Ok)
		{
			return false;
		}

		image = std::move(nextImage);
		currentImageIndex = index;
		return true;
	}

	void showAdjacentImage(HWND window, int direction)
	{
		if ((direction < 0 && currentImageIndex == 0) ||
			(direction > 0 && currentImageIndex + 1 >= images.size()))
		{
			return;
		}

		const std::size_t nextImageIndex = direction < 0 ? currentImageIndex - 1 : currentImageIndex + 1;
		if (loadImage(nextImageIndex))
		{
			InvalidateRect(window, nullptr, FALSE);
		}
	}

	void deleteCurrentImage(HWND window)
	{
		if (images.empty())
		{
			return;
		}

		const std::wstring imagePath = images[currentImageIndex].path;
		std::vector<wchar_t> recycleBinPath(imagePath.begin(), imagePath.end());
		recycleBinPath.push_back(L'\0');
		recycleBinPath.push_back(L'\0');
		image.reset();

		SHFILEOPSTRUCTW deleteOperation = {};
		deleteOperation.wFunc = FO_DELETE;
		deleteOperation.pFrom = recycleBinPath.data();
		deleteOperation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
		if (SHFileOperationW(&deleteOperation) != 0 || deleteOperation.fAnyOperationsAborted)
		{
			loadImage(currentImageIndex);
			return;
		}

		images.erase(images.begin() + currentImageIndex);
		if (images.empty())
		{
			currentImageIndex = 0;
			InvalidateRect(window, nullptr, FALSE);
			return;
		}

		if (currentImageIndex == images.size())
		{
			currentImageIndex = 0;
		}
		if (loadImage(currentImageIndex))
		{
			InvalidateRect(window, nullptr, FALSE);
		}
	}

	void drawImage(HDC deviceContext, const RECT& clientArea)
	{
		const int clientWidth = clientArea.right - clientArea.left;
		const int clientHeight = clientArea.bottom - clientArea.top;
		FillRect(deviceContext, &clientArea, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

		if (clientWidth <= 0 || clientHeight <= 0)
		{
			return;
		}
		if (!image)
		{
			Gdiplus::Graphics graphics(deviceContext);
			Gdiplus::FontFamily fontFamily(L"Segoe UI");
			Gdiplus::Font font(&fontFamily, 24.0f, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
			Gdiplus::SolidBrush textBrush(Gdiplus::Color::White);
			Gdiplus::StringFormat textFormat;
			textFormat.SetAlignment(Gdiplus::StringAlignmentCenter);
			textFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
			const Gdiplus::RectF textArea(0.0f, 0.0f, static_cast<Gdiplus::REAL>(clientWidth), static_cast<Gdiplus::REAL>(clientHeight));
			graphics.DrawString(L"No image found", -1, &font, textArea, &textFormat, &textBrush);
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
			if (wParam == VK_LEFT)
			{
				showAdjacentImage(window, -1);
				return 0;
			}
			if (wParam == VK_RIGHT)
			{
				showAdjacentImage(window, 1);
				return 0;
			}
			if (wParam == VK_DELETE)
			{
				deleteCurrentImage(window);
				return 0;
			}
			break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint = {};
			HDC deviceContext = BeginPaint(window, &paint);
			RECT clientArea = {};
			GetClientRect(window, &clientArea);
			const int clientWidth = clientArea.right - clientArea.left;
			const int clientHeight = clientArea.bottom - clientArea.top;
			HDC bufferContext = CreateCompatibleDC(deviceContext);
			HBITMAP bufferBitmap = CreateCompatibleBitmap(deviceContext, clientWidth, clientHeight);
			HGDIOBJ previousBitmap = SelectObject(bufferContext, bufferBitmap);
			drawImage(bufferContext, clientArea);
			BitBlt(deviceContext, 0, 0, clientWidth, clientHeight, bufferContext, 0, 0, SRCCOPY);
			SelectObject(bufferContext, previousBitmap);
			DeleteObject(bufferBitmap);
			DeleteDC(bufferContext);
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
	const std::wstring::size_type imageNameSeparator = imagePath.find_last_of(L"\\/");
	if (imageNameSeparator == std::wstring::npos)
	{
		MessageBoxW(nullptr, L"No se pudo identificar la imagen seleccionada.", L"ADN Viewer", MB_OK | MB_ICONERROR);
		Gdiplus::GdiplusShutdown(gdiplusToken);
		return 1;
	}

	const std::wstring selectedImageName = imagePath.substr(imageNameSeparator + 1);
	images = findImages(imagePath);
	const std::vector<ImageEntry>::const_iterator selectedImage = std::find_if(
		images.cbegin(),
		images.cend(),
		[&selectedImageName](const ImageEntry& candidate)
		{
			const std::wstring::size_type candidateNameSeparator = candidate.path.find_last_of(L"\\/");
			return candidateNameSeparator != std::wstring::npos &&
				lstrcmpiW(candidate.path.c_str() + candidateNameSeparator + 1, selectedImageName.c_str()) == 0;
		});
	if (selectedImage == images.cend() || !loadImage(static_cast<std::size_t>(selectedImage - images.cbegin())))
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