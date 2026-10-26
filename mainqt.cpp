#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileOpenEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QStringList>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace
{
	struct ImageEntry
	{
		QString path;
		QDateTime lastModified;
	};

	std::vector<ImageEntry> findImages(const QString& selectedImagePath)
	{
		const QFileInfo selectedImage(selectedImagePath);
		const QDir directory(selectedImage.absolutePath());
		if (!directory.exists())
		{
			return {};
		}

		std::vector<ImageEntry> foundImages;
		const QFileInfoList directoryEntries = directory.entryInfoList(
			QDir::Files | QDir::NoDotAndDotDot,
			QDir::NoSort);
		for (const QFileInfo& entry : directoryEntries)
		{
			if (entry.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) == 0)
			{
				foundImages.push_back({entry.absoluteFilePath(), entry.lastModified()});
			}
		}

		std::sort(foundImages.begin(), foundImages.end(), [](const ImageEntry& left, const ImageEntry& right)
		{
			if (left.lastModified != right.lastModified)
			{
				return left.lastModified > right.lastModified;
			}

			return QString::compare(left.path, right.path, Qt::CaseInsensitive) < 0;
		});
		return foundImages;
	}

	class ImageViewer final : public QWidget
	{
	public:
		explicit ImageViewer(std::vector<ImageEntry> images, std::size_t initialImageIndex)
			: images_(std::move(images)),
			  currentImageIndex_(initialImageIndex)
		{
			setFocusPolicy(Qt::StrongFocus);
			setAttribute(Qt::WA_OpaquePaintEvent);
			loadImage(initialImageIndex);
		}

		bool hasLoadedImage() const
		{
			return !image_.isNull();
		}

	protected:
		void keyPressEvent(QKeyEvent* event) override
		{
			switch (event->key())
			{
			case Qt::Key_Escape:
				close();
				return;

			case Qt::Key_Left:
				showAdjacentImage(-1);
				return;

			case Qt::Key_Right:
				showAdjacentImage(1);
				return;

			case Qt::Key_Delete:
				deleteCurrentImage();
				return;

			default:
				QWidget::keyPressEvent(event);
				return;
			}
		}

		void paintEvent(QPaintEvent* event) override
		{
			Q_UNUSED(event);

			QPainter painter(this);
			painter.fillRect(rect(), Qt::black);
			if (image_.isNull())
			{
				painter.setPen(Qt::white);
				painter.setFont(QFont(QStringLiteral("Helvetica Neue"), 24));
				painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("No image found"));
				return;
			}

			const QSize targetSize = image_.size().scaled(size(), Qt::KeepAspectRatio);
			const QPoint targetOrigin(
				(width() - targetSize.width()) / 2,
				(height() - targetSize.height()) / 2);
			painter.setRenderHint(QPainter::SmoothPixmapTransform);
			painter.drawImage(QRect(targetOrigin, targetSize), image_);
		}

	private:
		bool loadImage(std::size_t index)
		{
			if (index >= images_.size())
			{
				return false;
			}

			QImage nextImage;
			if (!nextImage.load(images_[index].path))
			{
				return false;
			}

			image_ = std::move(nextImage);
			currentImageIndex_ = index;
			return true;
		}

		void showAdjacentImage(int direction)
		{
			if ((direction < 0 && currentImageIndex_ == 0) ||
				(direction > 0 && currentImageIndex_ + 1 >= images_.size()))
			{
				return;
			}

			const std::size_t nextImageIndex = direction < 0
				? currentImageIndex_ - 1
				: currentImageIndex_ + 1;
			if (loadImage(nextImageIndex))
			{
				update();
			}
		}

		void deleteCurrentImage()
		{
			if (images_.empty())
			{
				return;
			}

			if (!QFile::moveToTrash(images_[currentImageIndex_].path))
			{
				return;
			}

			images_.erase(images_.begin() + static_cast<std::ptrdiff_t>(currentImageIndex_));
			if (images_.empty())
			{
				currentImageIndex_ = 0;
				image_ = QImage();
				update();
				return;
			}

			if (currentImageIndex_ == images_.size())
			{
				currentImageIndex_ = 0;
			}
			if (!loadImage(currentImageIndex_))
			{
				image_ = QImage();
			}
			update();
		}

		QImage image_;
		std::vector<ImageEntry> images_;
		std::size_t currentImageIndex_ = 0;
	};

	class ViewerApplication final : public QApplication
	{
	public:
		ViewerApplication(int& argc, char* argv[])
			: QApplication(argc, argv)
		{
		}

		bool openImage(const QString& imagePath)
		{
			const QFileInfo selectedImage(imagePath);
			if (!selectedImage.exists() || !selectedImage.isFile())
			{
				showOpenError(QStringLiteral("No se pudo identificar la imagen seleccionada."));
				return false;
			}

			std::vector<ImageEntry> images = findImages(selectedImage.absoluteFilePath());
			const QString selectedImagePath = selectedImage.absoluteFilePath();
			const std::vector<ImageEntry>::const_iterator selectedImageEntry = std::find_if(
				images.cbegin(),
				images.cend(),
				[&selectedImagePath](const ImageEntry& candidate)
				{
					return candidate.path == selectedImagePath;
				});
			if (selectedImageEntry == images.cend())
			{
				showOpenError(QStringLiteral("No se pudo cargar la imagen seleccionada."));
				return false;
			}

			const std::size_t selectedImageIndex = static_cast<std::size_t>(selectedImageEntry - images.cbegin());
			std::unique_ptr<ImageViewer> nextViewer = std::make_unique<ImageViewer>(
				std::move(images), selectedImageIndex);
			if (!nextViewer->hasLoadedImage())
			{
				showOpenError(QStringLiteral("No se pudo cargar la imagen seleccionada."));
				return false;
			}

			nextViewer->setWindowTitle(QStringLiteral("ADN Viewer"));
			if (QScreen* primaryScreen = QGuiApplication::primaryScreen())
			{
				nextViewer->setGeometry(primaryScreen->geometry());
			}
			viewer_ = std::move(nextViewer);
			viewer_->showFullScreen();
			viewer_->raise();
			viewer_->activateWindow();
			return true;
		}

	protected:
		bool event(QEvent* event) override
		{
			if (event->type() == QEvent::FileOpen)
			{
				const QFileOpenEvent* openEvent = static_cast<QFileOpenEvent*>(event);
				if (!openEvent->file().isEmpty())
				{
					openImage(openEvent->file());
				}
				return true;
			}

			return QApplication::event(event);
		}

	private:
		void showOpenError(const QString& message)
		{
			QMessageBox::critical(viewer_.get(), QStringLiteral("ADN Viewer"), message);
		}

		std::unique_ptr<ImageViewer> viewer_;
	};
}

int main(int argc, char* argv[])
{
	ViewerApplication application(argc, argv);
	const QStringList arguments = application.arguments();
	if (arguments.size() >= 2)
	{
		application.openImage(arguments.at(1));
	}
	return application.exec();
}