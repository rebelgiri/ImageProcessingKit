#include "im.h"
#include "ui_im.h"
#include <QDebug>
#include <QColor>
#include <QRgb>
#include <algorithm>
#include "qcustomplot.h"
#include <QtGlobal>
#include <ccomplex>
#include <QStack>
#include <QPoint>

im::im(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::im)
{
    ui->setupUi(this);

    // TOTO:
    // set mouse tracking, by default, mouse tracking is only enable the mouse button is pressed
    // however, I could not figure out why it doesn't work here.
    if (!hasMouseTracking()) {
        setMouseTracking(true);
    }

    // create new QGraphicsScene obeject, and binding to ui widget.
    inScene = new QGraphicsScenePlus;
    outScene = new QGraphicsScenePlus;
    ui->graphicsView_in->setScene(inScene);
    ui->graphicsView_out->setScene(outScene);

    // initialize a 1x1 white pixmap
    // and add to scene
    // so everything looks great
    inPixmap = new QPixmap(1, 1);
    inPixmap->fill();
    outPixmap = new QPixmap(*inPixmap);
    inPixmapItem = inScene->addPixmap(*inPixmap);
    outPixmapItem = outScene->addPixmap(*outPixmap);

    // connect signal and slot
    // once coord change, we emit a sinal from mouseMoveEvent
    // and then a slot is called to show the color value
    connect(inScene, SIGNAL(coordChanged(const QPointF&)), this, SLOT(showColorValue(const QPointF&)));
    // use bmp for compatiable for Windows
#ifdef Q_OS_WIN
      resultFileName = "tmp.bmp";
#else
    resultFileName = "tmp.png";
#endif
}

im::~im()
{
    delete ui;
}

// clear image diplayed in scene
void im::cleanImage()
{
    inScene->clear();
    outScene->clear();
}

void im::on_action_Quit_triggered()
{
    QApplication::quit();
}

void im::on_action_Open_triggered()
{
    QString imagePath = QFileDialog::getOpenFileName(
                this, tr("Open file"), QDir::homePath(), imageFormat);

    // check if file is valid
    if (!imagePath.isEmpty()) {
        QFile file(imagePath);

        if(!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, tr("Error"), tr("Unable to read image!"));
            return;
        }

        // clear previouly showed image
        cleanImage();

        // show image
        inPixmap->load(imagePath);
        inPixmapItem = inScene->addPixmap(*inPixmap);
        inScene->setSceneRect(QRectF(inPixmap->rect()));

        // save fileName for later use
        setFileName(imagePath);
    }
}

void im::on_action_Save_As_triggered()
{
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save image"), QDir::homePath(), imageFormat);

    if (!savePath.isEmpty()) {
        QFile file(savePath);

        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Error"), tr("Unable to save image!"));
            return;
        }

        // save output image to file
        outPixmapItem->pixmap().save(savePath);
        // update saveFileName, so that one could just save it
        setSaveFileName(savePath);
    }
}

void im::on_action_Close_triggered()
{
    cleanImage();
    // also set fileName to empty
    setFileName("");
}

void im::showColorValue(const QPointF &position)
{
    if (!fileName.isEmpty()) {
        CImg<unsigned char> img(fileName.toStdString().data());
        if (isRGB(img)) {
            // map position from scene to current item
            QPointF pos = inPixmapItem->mapFromScene(position);
            QPoint pixel;
            pixel.setX(int(pos.x()));
            pixel.setY(int(pos.y()));
            QRgb rgbValue = inPixmap->toImage().pixel(pixel);
            QColor color(rgbValue);
            int gray = qGray(rgbValue);
            QRect rect = inPixmap->rect();
            if (rect.contains(pixel)) {
                ui->label_coord->setText(tr("coord: %1, %2").arg(pixel.x()).arg(pixel.y()));
                ui->label_color_value->setText(tr("R: %1\tG: %2\tB: %3\tgray: %4").arg(color.red()).arg(color.green()).arg(color.blue()).arg(gray));
            }
        } else if(isGrayscale(img)) {
            // map position from scene to current item
            QPointF pos = inPixmapItem->mapFromScene(position);
            QPoint pixel;
            pixel.setX(int(pos.x()));
            pixel.setY(int(pos.y()));
            QRgb rgbValue = inPixmap->toImage().pixel(pixel);
            QColor color(rgbValue);
            int gray = qGray(rgbValue);
            QRect rect = inPixmap->rect();
            if (rect.contains(pixel)) {
                ui->label_coord->setText(tr("coord: %1, %2").arg(pixel.x()).arg(pixel.y()));
                ui->label_color_value->setText(tr("gray: %4").arg(gray));
            }
        } else {
            QMessageBox::critical(this, tr("Error!"), tr("Something is wrong."));
            return;
        }
    }
}

void im::adjustHsv(const int &h, const float &s, const float &v)
{
    CImg<float> img(fileName.toStdString().data());

    if (isRGB(img)) {
        // for RGB image, convert to HSV, adjust HSV
        // and then convert back to RGB
        img.RGBtoHSV();
        cimg_forXY(img, x, y) {
            img(x, y, 0) = std::fmod(img(x, y, 0) + h, 360);
            img(x, y, 1) = s*img(x, y, 1);
            img(x, y, 2) = v*img(x, y, 2);
        }
        img.HSVtoRGB().save(resultFileName.toStdString().data());
        updateOutScene(resultFileName);
    } else if (isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not an RGB image."));
        return;
    } else {
        QMessageBox::critical(this, tr("Error!"), tr("Unfortunately, something is wrong."));
        return;
    }
}

void im::linearTransformation(const double &k, const double &b)
{
    CImg<double> img(fileName.toStdString().data());

    if (isGrayscale(img)) {
        // grayscale image, just do it
        cimg_forXY(img, x, y) {
            img(x, y, 0) = img(x, y, 0)*k + b;
        }
    } else if (isRGB(img)) {
        // RGB image, convert to HSV, adjust V, convert back to RGB
        img.RGBtoHSV();
        cimg_forXY(img, x, y) {
            img(x, y, 2) = img(x, y, 2)*k + b;
        }
        img.HSVtoRGB();
    } else {
        QMessageBox::critical(this, tr("Error!"), tr("Unfortunately, something is wrong."));
        return;
    }

    img.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::piecewiseLinearTransformation(const double &r1, const double &s1, const double &r2, const double &s2)
{
    CImg<double> img(fileName.toStdString().data());

    if (isGrayscale(img)) {
        // for grayscale image, just do the transformation
        cimg_forXY(img, x, y) {
            if (img(x, y) < r1) {
                img(x, y) = s1/r1*img(x, y);
            } else if (img(x, y) < r2) {
                img(x, y) = (s2 - s1)/(r2 - r1)*(img(x, y) - r1) + s1;
            } else {
                img(x, y) = (255 - s2)/(255 - r1)*(img(x, y) - r2) + s2;
            }
        }
    } else if (isRGB(img)) {
        // for RGB image, convert to YUV, adjust Y
        // then convert back to RGB
        // why not HSV, and adjust V?
        // V range (0, 100%), Y range (0, 255)
        // the transformation assume gray range (0, 255)
        img.RGBtoYUV();
        cimg_forXY(img, x, y) {
            if (img(x, y, 0) < r1) {
                img(x, y, 0) = s1/r1*img(x, y, 0);
            } else if (img(x, y, 2) < r2) {
                img(x, y, 0) = (s2 - s1)/(r2 - r1)*(img(x, y, 0) - r1) + s1;
            } else {
                img(x, y, 0) = (255 - s2)/(255 - r1)*(img(x, y, 0) - r2) + s2;
            }
        }
        img.YUVtoRGB();
    } else {
        QMessageBox::critical(this, tr("Error!"), tr("Unfortunately, something is wrong."));
        return;
    }

    img.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);

}

void im::averageFilter(const int &size)
{
    CImg<float> img(fileName.toStdString().data());
    CImg<float> kernel(size, size, 1, 1, 1.0f/size/size);

    CImg<float> dest = img.convolve(kernel);
    dest.save(resultFileName.toStdString().data());

    updateOutScene(resultFileName);
}

void im::medianFilter(const int &size)
{
    CImg<float> img(fileName.toStdString().data());
    img.blur_median(size);
    img.save(resultFileName.toStdString().data());

    updateOutScene(resultFileName);
}

// one might consider using dilate & erode function from CImg
// they're faster with some optimization
// I don't know that maximum & minimum filters are usually know as dilate & erode until now
// sedd https://github.com/dtschump/CImg/issues/171
//
// maximum filter
// with size x size window
// just slide window over the image
// replace the middle pixel's intensity with
// maximum of the window
void im::maximumFilter(const int &size)
{
    CImg<unsigned char> img(fileName.toStdString().data());
    // slide window, just like a kernel, but not for convolve
    // so it's not a good idea to name it kernel
    // use window instead
    CImg<unsigned char> window(size, size);
    CImg<unsigned char> dest(img);

    cimg_forXY(img, x, y) {
        cimg_forXY(window, u, v) {
            window(u, v) = img(x + u, y + v);
        }
        // (x + (size - 1)/2, y + (size - 1)/2) is the middle pos
        // of the slide window
        // check if it's out of array boundary
        if (x + (size - 1)/2 < img.width() && y + (size - 1)/2 < img.height()) {
            dest(x, y) = window.max();
            qDebug() << dest(x, y) << endl;
        }
    }

    dest.save(resultFileName.toStdString().data());

    updateOutScene(resultFileName);
}

// minimum filter, just like maximum filter
// but use minimum instead of maximum
void im::minimumFilter(const int &size)
{
    CImg<unsigned char> img(fileName.toStdString().data());
    CImg<unsigned char> window(size, size, 1, 1, 255);
    CImg<unsigned char> dest(img);

    cimg_forXY(img, x, y) {
        cimg_forXY(window, u, v) {
            window(u, v) = img(x + u, y + v);
        }
        if (x + (size - 1)/2 < img.width() && y + (size - 1)/2 < img.height()) {
            dest(x + (size - 1)/2, y + (size - 1)/2) = window.min();
        }
    }

    dest.save(resultFileName.toStdString().data());

    updateOutScene(resultFileName);
}

void im::invertFilter(const int &noiseType,
                      const int &D0,
                      const double &variance,
                      const int &length,
                      const int &angle)
{
    CImg<double> psf = getPsfKernel(length, angle);

    CImg<double> img(fileName.toStdString().data());
    CImg<double> imgMotionBlured(img);

    if (noiseType == 0) {
        imgMotionBlured = img.get_convolve(psf, img).get_normalize(0, 255);
    } else if (noiseType == 1) {
        imgMotionBlured = img.get_convolve(psf, img).get_normalize(0, 255);
        imgMotionBlured.noise(variance);
    } else {
        QMessageBox::critical(this, tr("Error!"), tr("Unknown noise type."));
        return;
    }

    CImgList<double> G = imgMotionBlured.get_FFT();
    G = fftshift(G);

    CImgList<double> H = psfToOtf(psf, img.width(), img.height());
    H = fftshift(H);

    CImgList<double> F = div(G, H, D0);
    F = fftshift(F);
    CImg<double>::FFT(F[0], F[1], true);
    F[0].normalize(0, 255);
    F[0].save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::customFilter(const int &w00, const int &w01, const int &w02, const int &w10, const int &w11, const int &w12, const int &w20, const int &w21, const int &w22)
{
    CImg<float> img(fileName.toStdString().data());
    int size = 3;
    CImg<float> kernel(size, size, 1, 1,
                       w00, w01, w02,
                       w10, w11, w12,
                       w20, w21, w22);
    // if sum of all pixels in kernel is not zero
    // make sure it's 1
    if (!(kernel.sum() < DBL_EPSILON)) {
        kernel = kernel/(size*size);
    }

    img.convolve(kernel);
    img.save(resultFileName.toStdString().data());

    updateOutScene(resultFileName);
}

void im::resize(const double &wFactor, const double &hFactor, const int &interpolationType)
{
    CImg<double> img(fileName.toStdString().data());

    qDebug() << "Interpolation Type:" << interpolationType << endl;
    CImg<double> result = img.get_resize(static_cast<int>(round(img.width()*wFactor)),
                                         static_cast<int>(round(img.height()*hFactor)),
                                         img.depth(), img.spectrum(), interpolationType);
    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::threshold(const int &threshold)
{
    CImg<unsigned char> img(fileName.toStdString().data());

    if (!isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error"), tr("Non-grayscale image."));
        return;
    }

    CImg<unsigned char> result(img);

    cimg_forXY(result, x, y) {
        if (img(x, y) <= threshold) {
            result(x, y) = 0;
        } else {
            result(x, y) = 255;
        }
    }

    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::erode(unsigned char structureElement[3][3])
{
    CImg<unsigned char> s(3, 3);

    cimg_forXY(s, x, y) {
        s(x, y) = structureElement[x][y];
    }

    CImg<double> img(fileName.toStdString().data());
    CImg<double> result = img.get_erode(s);
    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::regionGrowth(const QPoint &seed, const int &threshold)
{
    CImg<int> img(fileName.toStdString().data());
    CImg<int> result(img.width(), img.height(), img.depth(), img.spectrum(), 255);
    int T = threshold;
    int x, y;
    QStack<QPoint> seeds;
    QPoint currentSeed;
    seeds.push(seed);
    result(seed.x(), seed.y()) = 0;

    while(!seeds.isEmpty()) {
        currentSeed = seeds.pop();
        x = currentSeed.x();
        y = currentSeed.y();
        for (int i = -1; i < 2; ++i) {
            for (int j = -1; j < 2; ++j) {
                if (isInsideImage(QPoint(x + i, y + j), img)) {
                    if (result(x + i, y + j) != 0) {
                        if (abs(img(x + i, y + j) - img(x, y)) < T) {
                            seeds.push(QPoint(x + i, y + j));
                            result(x + i, y + j) = 0;
                        }
                    }
                }
            }
        }
    }

    //qDebug() << img(100, 100) << endl;
    //qDebug() << img(101, 100) << endl;

    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::dilate(unsigned char structureElement[3][3])
{
    CImg<unsigned char> s(3, 3);

    cimg_forXY(s, x, y) {
        s(x, y) = structureElement[x][y];
    }

    CImg<double> img(fileName.toStdString().data());
    CImg<double> result = img.get_dilate(s);

    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::opening(unsigned char structureElement[3][3])
{
    CImg<unsigned char> s(3, 3);

    cimg_forXY(s, x, y) {
        s(x, y) = structureElement[x][y];
    }

    CImg<double> img(fileName.toStdString().data());
    CImg<double> result = img.get_erode(s).get_dilate(s);

    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::closing(unsigned char structureElement[3][3])
{
    CImg<unsigned char> s(3, 3);

    cimg_forXY(s, x, y) {
        s(x, y) = structureElement[x][y];
    }

    CImg<double> img(fileName.toStdString().data());
    CImg<double> result = img.get_dilate(s).get_erode(s);

    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::idealHighPassFilter(const int &D0)
{
    CImg<double> img(fileName.toStdString().data());

    // only deal with grayscale image
    if (!isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Non-grayscale image!"));
        return;
    }

    CImgList<double> F = img.get_FFT();
    // shift FFT result
    F[0] = fftshift(F[0]);
    F[1] = fftshift(F[1]);

    CImgList<double> H(2, img.width(), img.height(), img.depth(), img.spectrum(), 0.0f);

    // generate H
    double D;
    cimg_forXY(img, x, y) {
        D = sqrt((x - img.width()/2)*(x - img.width()/2)
                 + (y - img.height()/2)*(y - img.height()/2));
        if (D > D0) {
            H[0](x, y) = 1.0f;
        }
    }

    CImgList<double> G = mul(H[0], H[1], F[0], F[1]);
    // shift
    G[0] = fftshift(G[0]);
    G[1] = fftshift(G[1]);
    CImg<double>::FFT(G[0], G[1], true);

    G[0].normalize(0, 255);
    G[0].save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::idealLowPassFilter(const int &D0)
{
    CImg<double> img(fileName.toStdString().data());

    // only deal with grayscale image
    if (!isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Non-grayscale image!"));
        return;
    }

    CImgList<double> F = img.get_FFT();
    CImgList<double> H(2, img.width(), img.height(), img.depth(), img.spectrum(), 0.0f);

    // generate H
    double D;
    cimg_forXY(img, x, y) {
        D = sqrt((x - img.width()/2)*(x - img.width()/2) + (y - img.height()/2)*(y - img.height()/2));
        if (D <= D0) {
            H[0](x, y) = 1.0f;
        }
    }
    // shift FFT result
    F[0] = fftshift(F[0]);
    F[1] = fftshift(F[1]);

    CImgList<double> G = mul(H[0], H[1], F[0], F[1]);
    // shift
    G[0] = fftshift(G[0]);
    G[1] = fftshift(G[1]);
    // IFFT
    CImg<double>::FFT(G[0], G[1], true);
    G[0].normalize(0, 255);
    G[0].save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::butterworthLowPassFilter(const int &Order, const int &D0)
{
    CImg<double> img(fileName.toStdString().data());

    // only deal with grayscale image
    if (!isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Non-grayscale image!"));
        return;
    }

    CImgList<double> F = img.get_FFT();
    // shift
    F[0] = fftshift(F[0]);
    F[1] = fftshift(F[1]);
    // generate H
    double D;
    CImgList<double> H(2, img.width(), img.height(), img.depth(), img.spectrum(), 0.0f);

    cimg_forXY(img, x, y) {
        D = sqrt((x - img.width()/2)*(x - img.width()/2)
                 + (y - img.height()/2)*(y - img.height()/2));
        H[0](x, y) = 1/(1 + pow(D/D0, 2*Order));
    }

    CImgList<double> G = mul(F[0], F[1], H[0], H[1]);

    // shift
    G[0] = fftshift(G[0]);
    G[1] = fftshift(G[1]);
    // IFFT
    CImg<double>::FFT(G[0], G[1], true);
    qDebug() << "min: " << G[0].min() <<
                "max: " << G[0].max() << endl;
    G[0].normalize(0, 255);
    G[0].save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::butterworthHighPassFilter(const int &Order, const int &D0)
{
    CImg<double> img(fileName.toStdString().data());

    // only deal with grayscale image
    if (!isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Non-grayscale image!"));
        return;
    }

    CImgList<double> F = img.get_FFT();
    // shift
    F[0] = fftshift(F[0]);
    F[1] = fftshift(F[1]);
    // generate H
    double D;
    CImgList<double> H(2, img.width(), img.height(), img.depth(), img.spectrum(), 0.0f);

    cimg_forXY(img, x, y) {
        D = sqrt((x - img.width()/2)*(x - img.width()/2) + (y - img.height()/2)*(y - img.height()/2));
        H[0](x, y) = 1/(1 + pow(D0/D, 2*Order));
    }

    CImgList<double> G = mul(F[0], F[1], H[0], H[1]);

    // shift
    G[0] = fftshift(G[0]);
    G[1] = fftshift(G[1]);
    // IFFT
    CImg<double>::FFT(G[0], G[1], true);
    qDebug() << "min: " << G[0].min() <<
                "max: " << G[0].max() << endl;
    G[0].normalize(0, 255);
    G[0].save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::homomorphicFilter(const double &gammaL, const double &gammaH, const double &c, const int &D0)
{
    CImg<double> img(fileName.toStdString().data());

    // only deal with grayscale image
    if (!isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Non-grayscale image!"));
        return;
    }
    // img's gray level might be 0, which make it no sence
    // so add 1 before log
    img = log(1 + img);
    // FFT
    CImgList<double> F = img.get_FFT();
    F[0] = fftshift(F[0]);
    F[1] = fftshift(F[1]);
    // generate H
    CImgList<double> H(2, img.width(), img.height(), 1, 1, 0.0f);
    double D;

    cimg_forXY(img, u, v) {
        D = sqrt((u - img.width()/2)*(u - img.width()/2) + (v - img.height()/2)*(v - img.height()/2));
        H[0](u, v) = (gammaH - gammaL)*(1 - std::exp(-c*(D/D0)*(D/D0))) + gammaL;
    }

    CImgList<double> G = mul(F[0], F[1], H[0], H[1]);
    G[0] = fftshift(G[0]);
    G[1] = fftshift(G[1]);
    CImg<double>::FFT(G[0], G[1], true);
    // only the real part matters, ignore imag part
    G[0] = G[0].exp() - 1;
    qDebug() << "max: " << G[0].max() << ", min: " << G[0].min() << endl;
    G[0].normalize(0, 255);
    G[0].save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::motionBlur(const int &length, const int &angle)
{
    CImg<double> psf = getPsfKernel(length, angle);
    CImg<double> img(fileName.toStdString().data());
    CImg<double> result = img.get_convolve(psf);
    result.normalize(0, 255).save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::gaussianNoise(const double &variance)
{
    CImg<double> img(fileName.toStdString().data());
    CImg<double> result = img.get_noise(variance).get_normalize(0, 255);

    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::atmosphericCirculationBlur(const double &k)
{
    CImg<double> img(fileName.toStdString().data());
    CImgList<double> F = img.get_FFT();
    F = fftshift(F);

    // generate H
    //CImgList<double> H(2, img.width(), img.height(), 1, 1, 0.0f);
    CImgList<double> H(F, 0.0f);
    double D;

    cimg_forXY(H[0], x, y) {
        D = (x - img.width()/2)*(x - img.width()/2)
                 + (y - img.height()/2)*(y - img.height()/2);
        H[0](x, y) = exp(-k*pow(D, 5.0f/6.0f));
        H[1](x, y) = 0.0f;
    }

    CImgList<double> G(F);
    std::complex<double> tmp1, tmp2, tmp3;

    cimg_forXY(H[0], x, y) {
        tmp1 = std::complex<double>(F[0](x, y), F[1](x, y));
        tmp2 = std::complex<double>(H[0](x, y), H[1](x, y));
        tmp3 = tmp1*tmp2;
        G[0](x, y) = tmp3.real();
        G[1](x, y) = tmp3.imag();
    }
    G = fftshift(G);
    CImg<double>::FFT(G[0], G[1], true);
    G[0].normalize(0, 255);
    G[0].save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::wienerFilter(const int &noiseType,
                  const double &variance,
                  const int &length,
                  const int &angle,
                  const double &k)
{
    CImg<double> img(fileName.toStdString().data());
    CImg<double> psf = getPsfKernel(length, angle);
    CImg<double> imgMotionBlur(img);

    if (noiseType == 0) {
        imgMotionBlur = img.get_convolve(psf);
    } else if (noiseType == 1) {
        imgMotionBlur = img.get_convolve(psf);
        imgMotionBlur = imgMotionBlur.get_noise(variance);
    } else {
        QMessageBox::critical(this, tr("Error!"), tr("Unknown noise type."));
        return;
    }

    CImgList<double> H = psfToOtf(psf, img.width(), img.height());
    CImgList<double> HConj = getConj(H);
    CImgList<double> H2 = mul(H, HConj);
    CImgList<double> dem(H2);

    dem = add(dem, std::complex<double>(k, 0));

    double S_x = img.variance();
    qDebug() << "K:" << S_x/variance << endl;
    CImgList<double> G = imgMotionBlur.get_FFT();
    CImgList<double> F = mul(div(HConj, dem), G);
    CImg<double>::FFT(F[0], F[1], true);
    F[0].normalize(0, 255).save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::ifft(const int &ifftType)
{
    CImg<double> img(fileName.toStdString().data());
    // fft forward
    CImgList<double> F = img.get_FFT();
    CImgList<double> magnitude = keepMagnitude(F);
    CImgList<double> phase = keepPhase(F);
    // fft backward, aka ifft
    if (ifftType == 0) {
        CImg<double>::FFT(F[0], F[1], true);
        // take only real part, and normalize to (0, 255)
        F[0].normalize(0, 255).save(resultFileName.toStdString().data());
    } else if (ifftType == 1) {
        CImg<double>::FFT(magnitude[0], magnitude[1], true);
        magnitude[0].normalize(0, 255);
        qDebug() << "max: " << magnitude[0].max() << endl;
        qDebug() << "min: " << magnitude[0].min() << endl;
        // take only real part, and normalize to (0, 255)
        magnitude[0].save(resultFileName.toStdString().data());
    } else if (ifftType == 2) {
        CImg<double>::FFT(phase[0], phase[1], true);
        // take only real part, and normalize to (0, 255)
        phase[0].normalize(0, 255).save(resultFileName.toStdString().data());
    } else {
        QMessageBox::critical(this, tr("Error!"), tr("Unknown IFFT Type."));
        return;
    }

    updateOutScene(resultFileName);
}

void im::setFileName(const QString &fileName)
{
    this->fileName = fileName;
}

void im::setSaveFileName(const QString &saveFileName)
{
    this->saveFileName = saveFileName;
}

void im::updateOutScene(const QString &fileName)
{
    qDebug() << "repaint out image." << endl;
    outScene->clear();
    outPixmap->load(fileName);
    outPixmapItem = outScene->addPixmap(*outPixmap);
    outScene->setSceneRect(QRectF(outPixmap->rect()));
}

// convert RGB to gray scale
// note the formular here assume that
// RGB weight differs.
int im::rgbToGray(const int &r, const int &g, const int &b)
{
    return (r * 11 + g * 16 + b * 5)/32;
}

// create motion blur psf kernel
// according to GNU Octave's fspecial implement
// just generate a horizontal line across the middle
// and then rotate to the specific angle
CImg<double> im::getPsfKernel(const int &length, const int &angle)
{
    int len;
    if (length % 2 == 0) {
        len = length + 1;
    } else {
        len = length;
    }

    CImg<double> psf(len, len, 1, 1, 0.0f);

    cimg_forX(psf, x) {
        psf(x, len/2) = 1;
    }
    psf.rotate(-angle);
    psf /= psf.sum();

    return psf;
}

bool im::isBinary(const CImg<int> &img)
{
    bool result = true;

    if (!isGrayscale(img)) {
        return false;
    }

    cimg_forXY(img, x, y) {
        if (img(x, y) != 0 && img(x, y) != 255) {
            result = false;
        }
    }

    return result;
}

CImg<int> im::operatorAnd(const CImg<int> &img1, const CImg<int> &img2)
{
    CImg<int> result(img1);

    cimg_forXY(result, x, y) {
        if (img1(x, y) == 255 && img2(x, y) == 255) {
            result(x, y) = 255;
        } else {
            result(x, y) = 0;
        }
    }

    return result;
}

CImg<int> im::operatorOr(const CImg<int> &img1, const CImg<int> &img2)
{
    CImg<int> result(img1);

    cimg_forXY(result, x, y) {
        if (img1(x, y) == 0 && img2(x, y) == 0) {
            result(x, y) = 0;
        } else {
            result(x, y) = 255;
        }
    }

    return result;
}

CImg<int> im::operatorXor(const CImg<int> &img1, const CImg<int> &img2)
{
    CImg<int> result(img1);

    cimg_forXY(result, x, y) {
        if ((img1(x, y) == 0 && img2(x, y) == 0)
                || (img1(x, y) == 255 && img2(x, y) == 255)) {
            result(x, y) = 0;
        } else {
            result(x, y) = 255;
        }
    }

    return result;
}

template<typename T>
CImgList<T> im::mul(const CImg<T> &img1_real, const CImg<T> &img1_imag, const CImg<T> &img2_real, const CImg<T> &img2_imag)
{
    std::complex<T> tmp1, tmp2, tmp3;
    CImgList<T> result(2, img1_real);

    cimg_forXY(result[0], x, y) {
        tmp1 = std::complex<double>(img1_real(x, y), img1_imag(x, y));
        tmp2 = std::complex<double>(img2_real(x, y), img2_imag(x, y));
        tmp3 = tmp1*tmp2;
        result[0](x, y) = tmp3.real();
        result[1](x, y) = tmp3.imag();
    }

    return result;
}

template <typename T>
CImgList<T> im::mul(const CImgList<T> &img1, const CImgList<T> &img2)
{
    std::complex<T> tmp1, tmp2, tmp3;
    CImgList<T> result(img1);

    cimg_forXY(result[0], x, y) {
        tmp1 = std::complex<T>(img1[0](x, y), img1[1](x, y));
        tmp2 = std::complex<T>(img2[0](x, y), img1[1](x, y));
        tmp1 = tmp1*tmp2;
        result[0](x, y) = tmp3.real();
        result[1](x, y) = tmp3.imag();
    }

    return result;
}

CImg<double> im::magnitude(const CImgList<double> &img)
{
    return log(1 + sqrt((log(1 + sqrt(img[0].get_mul(img[0]) + img[1].get_mul(img[1]))))));
}

void im::on_action_Adjust_HSV_triggered()
{
    dialogAdjustHsv = new DialogAdjustHsv;
    dialogAdjustHsv->setModal(true);
    dialogAdjustHsv->show();

    connect(dialogAdjustHsv, SIGNAL(sendHsvData(int, float, float)), this, SLOT(adjustHsv(int, float, float)));
}

void im::on_action_Grayscale_triggered()
{
    // read RGB file
    CImg<int> img(fileName.toStdString().data());

    if (isGrayscale(img)) {
        // for grayscale image, do nothing
        QMessageBox::critical(this, tr("Error!"), tr("Not an RGB image!"));
        return;
    } else if (isRGB(img)) {
        // for RGB image, convert to grayscale
        // create a gray scale image
        CImg<int> dest(img.width(), img.height());

        // convert RGB to gray scale, store in dest
        cimg_forXY(dest, x, y) {
            dest(x, y) = rgbToGray(img(x, y, 0), img(x, y, 1), img(x, y, 2));
        }

        // save image
        dest.save(resultFileName.toStdString().data());
        updateOutScene(resultFileName);
    } else {
        QMessageBox::critical(this, tr("Error!"), tr("Unfortunately, something is wrong."));
        return;
    }
}

void im::on_action_Linear_Transformation_triggered()
{
    dialogLinearTransform = new DialogLinearTransform;
    dialogLinearTransform->setModal(true);
    dialogLinearTransform->show();

    connect(dialogLinearTransform, SIGNAL(sendData(double,double)), this, SLOT(linearTransformation(double,double)));
}

void im::on_action_Histogram_triggered()
{
    CImg<float> img(fileName.toStdString().data());
    // get histogam
    CImg<float> hist = img.histogram(255);
    // set title
    QString title = "Histogram of " + fileName;
    // create an object to show window
    CImgDisplay display(800, 600, title.toStdString().data());
    // show histogram
    hist.display_graph(display, 3);
}

void im::on_action_Histogram_Equalization_triggered()
{
    // we need CImg<T = unsigned int>, using unsigned char is wrong
    // not sure why yet
    CImg<unsigned int> img(fileName.toStdString().data());
    CImg<unsigned int> dest(img);
    // do the histogram equalization
//    CImg<int> dest(img.width(), img.height());
//    cimg_forXY(dest, x, y) {
//        dest(x, y) = map[img(x, y)];
//    }
    if (isGrayscale(img)) {
        QMap<unsigned int, unsigned int> map = getHistogramEqualizationMap(img);
        cimg_forXY(img, x, y) {
            if (map.contains(img(x, y))) {
                dest(x, y) = map.value(img(x, y));
            }
        }
    } // else if (isRGB(img)) {
    //        CImg<unsigned int> R(img.width(), img.height()),
    //                G(img.width(), img.height()),
    //                B(img.width(), img.height());
    //        QMap<unsigned int, unsigned int> mapR = getHistogramEqualizationMap(R),
    //                mapG = getHistogramEqualizationMap(G),
    //                mapB = getHistogramEqualizationMap(B);

    //        cimg_forXY(img, x, y) {
    //            if (mapR.contains(img(x, y, 0))) {
    //                dest(x, y, 0) = mapR.value(img(x, y, 0));
    //            }

    //            if (mapG.contains(img(x, y, 0))) {
    //                dest(x, y, 0) = mapG.value(img(x, y, 0));
    //            }

    //            if (mapB.contains(img(x, y, 0))) {
    //                dest(x, y, 0) = mapB.value(img(x, y, 0));
    //            }
    //        }
        else {
            QMessageBox::critical(this, tr("Error!"), tr("Not a grayscale image."));
            return;
    }

    dest.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_Histogram_Specification_triggered()
{
    CImg<unsigned char> img(fileName.toStdString().data());
    // for non-grayscale image, do nothing, just return
    if (!isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not a grayscale image."));
        return;
    }
    // open reference file
    QString refPath = QFileDialog::getOpenFileName(
                this, tr("Choose reference image file"), QDir::homePath(), imageFormat);

    if (!refPath.isEmpty()) {
        QFile file(refPath);

        if(!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, tr("Error"), tr("Unable to read reference image!"));
            return;
        }
    }
    CImg<unsigned char> ref(refPath.toStdString().data());
    // refernce image shall be grayscale too
    if (!isGrayscale(ref)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not a grayscale image."));
        return;
    }
    // get mapping
    QMap<unsigned char, unsigned char> map = getHistogramSpecificationMap(img, ref);
    // do the thing
    CImg<unsigned char> dest(img);
    cimg_forXY(dest, x, y) {
        if (map.contains(img(x, y))) {
            dest(x, y) = map.value(img(x, y));
        }
    }

    dest.save(resultFileName.toStdString().data());

    updateOutScene(resultFileName);
}

void im::on_action_Piecewise_Linear_Transformation_triggered()
{
    dlgPiecewiseLinearTranformation = new DialogPiecewiseLinearTransformation;
    dlgPiecewiseLinearTranformation->setModal(true);
    dlgPiecewiseLinearTranformation->show();

    connect(dlgPiecewiseLinearTranformation, SIGNAL(sendData(double, double, double, double)), this, SLOT(piecewiseLinearTransformation(double, double, double, double)));
}

void im::on_action_Average_Filter_triggered()
{
    dlgAverageFilter = new DialogAvarageFilter;
    dlgAverageFilter->setModal(true);
    dlgAverageFilter->show();

    connect(dlgAverageFilter, SIGNAL(sendData(int)), this, SLOT(averageFilter(int)));
}

void im::on_action_Laplacian_Filter_triggered()
{
    CImg<float> img(fileName.toStdString().data());
    CImg<float> dest = img.get_laplacian();
    dest = img + 0.5*dest;

    dest.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_Median_Filter_triggered()
{
    dlgMedianFilter = new DialogMedianFilter;
    dlgMedianFilter->setModal(true);
    dlgMedianFilter->show();

    connect(dlgMedianFilter, SIGNAL(sendData(int)), this, SLOT(medianFilter(int)));
}

void im::on_action_Maximum_Filter_triggered()
{
    dlgMaximumFilter = new DialogMaximumFilter;
    dlgMaximumFilter->setModal(true);;
    dlgMaximumFilter->show();

    connect(dlgMaximumFilter, SIGNAL(sendData(int)), this, SLOT(maximumFilter(int)));
}

void im::on_action_Minimum_Filter_triggered()
{
    dlgMinimumFilter = new DialogMinimumFilter;
    dlgMinimumFilter->setModal(true);
    dlgMinimumFilter->show();

    connect(dlgMinimumFilter, SIGNAL(sendData(int)), this, SLOT(minimumFilter(int)));

}

void im::on_action_Custom_Filter_triggered()
{
    dlgCustomFilter = new DialogCustomFilter;
    dlgCustomFilter->setModal(true);
    dlgCustomFilter->show();

    connect(dlgCustomFilter, SIGNAL(sendData(int,int,int,int,int,int,int,int,int)),
            this, SLOT(customFilter(int,int,int,int,int,int,int,int,int)));
}

void im::on_action_Pseudocolor_triggered()
{
    CImg<float> img(fileName.toStdString().data());
    CImg<float> dest(img.width(), img.height(), 1, 3);

    // transformation for R channel
    cimg_forXY(img, x, y) {
        if (img(x, y) < 128) {
            dest(x, y, 0, 0) = 0;
        } else if (img(x, y) < 200) {
            dest(x, y, 0, 0) = 255/128*(img(x, y) - 128);
        } else {
            dest(x, y, 0, 0) = 255;
        }
    }

    // transformation for G channel
    cimg_forXY(img, x, y) {
        if (img(x, y) < 64) {
            dest(x, y, 0, 1) = 255/64*img(x, y);
        } else if (img(x, y) < 200) {
            dest(x, y, 0, 1) = 255;
        } else {
            dest(x, y, 0, 1) = -255/(255 - 200)*(img(x, y) - 200) + 255;
        }
    }

    // transformation for B channel
    cimg_forXY(img, x, y) {
        if (img(x, y) < 64) {
            dest(x, y, 0, 2) = 255;
        } else if (img(x, y) < 128) {
            dest(x, y, 0, 2) = -255/(128 - 64)*(img(x, y) - 64) + 255;
        } else {
            dest(x, y, 0, 2) = 0;
        }
    }

    dest.save(resultFileName.toStdString().data());

    updateOutScene(resultFileName);
}

template<typename T>
QMap<T, T> im::getHistogramEqualizationMap(const CImg<T> &img, const int &nLevel)
{
    double imageSize = img.width()*img.height();
    // create mapping
    // <int, int> = <old, new>
    QMap<T, T> map;
    // compute histogram
    CImg<T> histogram = img.get_histogram(nLevel);
    // compute cdf
    //    for(int x = 1; x < histogram.width(); ++x) {
    //        histogram(x) += histogram(x - 1);
    //    }
    // use cumulate method from CImg to compute cdf, it's faster
    histogram.cumulate("x");
    cimg_forX(histogram, x) {
        map.insert(x, histogram(x)*(nLevel - 1)/imageSize);
    }

    return map;
}

// It seems that one shall not use template here
// if he or she is not that crazy about tempate.
// But, I will just keep it this way.
template<typename T>
QMap<T, T> im::getHistogramSpecificationMap(const CImg<T> &src, const CImg<T> &ref, const int &nLevel)
{
    // compute histogram
    CImg<unsigned int long> srcHistogram = src.get_histogram(nLevel);
    CImg<unsigned int long> refHistogram = ref.get_histogram(nLevel);
    // compute cdf
    srcHistogram.cumulate("x");
    refHistogram.cumulate("x");
    double sizeSrc = src.width()*src.height();
    double sizeRef = ref.width()*ref.height();
    // get norm cdf
    CImg<double> cdfSrc = srcHistogram/sizeSrc;
    CImg<double> cdfRef = refHistogram/sizeRef;
//    cdfSrc.display_graph(0, 3);
//    cdfRef.display_graph(0, 3);
    //
    QMap<T, T> map;
    int index;
    CImg<double> tmp(cdfSrc);
    cimg_forX(cdfSrc, x) {
        tmp = abs(cdfSrc(x) - cdfRef);
        index = getMinimumPos(tmp);
        map.insert(x, index);
    }

    return map;
}

template<typename T>
int im::getMinimumPos(const CImg<T> &img)
{
    int pos = 0;
    for(int x = 0; x < img.width(); ++x) {
        if (img(x) < img(pos)) {
            pos = x;
        }
    }

    return pos;
}

template<typename T>
bool im::isGrayscale(const CImg<T> &img)
{
    if (img.spectrum() == 1) {
        return true;
    } else {
        return false;
    }
}

template<typename T>
bool im::isRGB(const CImg<T> &img)
{
    if (img.spectrum() == 3) {
        return true;
    } else {
        return false;
    }
}

void im::on_action_Save_triggered()
{
    if (!saveFileName.isEmpty()) {
        // if saveFileName is not empty
        // just save output image to saveFileName
        outPixmapItem->pixmap().save(saveFileName);
    } else {
        // else call save as function
        on_action_Save_As_triggered();
    }
}

// about Qt
// just call QApplication::aboutQt()
// to show some information
void im::on_action_About_Qt_triggered()
{
    QApplication::aboutQt();
}

// about dialog
void im::on_action_About_ImageProcessingKit_triggered()
{
    QMessageBox::about(this, tr("About - ImageProcessingKit"), tr("<H1>ImageProcessingKit</H1> Power by Qt."));
}

void im::on_action_Addition_triggered()
{
    QStringList tmpFiles = QFileDialog::getOpenFileNames(this, tr("Open File(s)"), QDir::homePath(), imageFormat);

    if (!tmpFiles.isEmpty()) {
        CImg<double> img(fileName.toStdString().data());

        for(int i = 0; i < tmpFiles.count(); ++i) {
            CImg<double> tmp(tmpFiles.at(i).toStdString().data());
            // resize image before operation
            tmp.resize(img.width(), img.height(), img.depth(), img.spectrum());
            img += tmp;
            img /= 2;
        }

        img.save(resultFileName.toStdString().data());
        updateOutScene(resultFileName);
    }
}

void im::on_action_Subtraction_triggered()
{
    QString tmpFile = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), imageFormat);

    if (!tmpFile.isEmpty()) {
        CImg<double> tmpImg(tmpFile.toStdString().data());
        CImg<double> img(fileName.toStdString().data());

        // resize image before operation
        tmpImg.resize(img.width(), img.height(), img.depth(), img.spectrum());
        img -= tmpImg;
        img.save(resultFileName.toStdString().data());
        updateOutScene(resultFileName);
    }
}

void im::on_action_Multiplication_triggered()
{
    QString tmpFile = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), imageFormat);

    if (!tmpFile.isEmpty()) {
        CImg<double> tmpImg(tmpFile.toStdString().data());
        CImg<double> img(fileName.toStdString().data());

        tmpImg.resize(img.width(), img.height(), img.depth(), img.spectrum());
        img.mul(tmpImg);
        img.save(resultFileName.toStdString().data());
        updateOutScene(resultFileName);
    }
}

void im::on_action_Division_triggered()
{
    QString tmpFile = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), imageFormat);

    if (!tmpFile.isEmpty()) {
        CImg<double> tmpImg(tmpFile.toStdString().data());
        CImg<double> img(fileName.toStdString().data());

        tmpImg.resize(img.width(), img.height(), img.depth(), img.spectrum());
        img.div(tmpImg);
        img.normalize(0, 255);
        img.save(resultFileName.toStdString().data());
        updateOutScene(resultFileName);
    }
}

void im::on_action_Negative_triggered()
{
    CImg<int> img(fileName.toStdString().data());

    img = 255 - img;
    img.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_XOR_triggered()
{
    CImg<int> img(fileName.toStdString().data());

    if (!isBinary(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not binary image"));
        return;
    }

    QString tmpFile = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), imageFormat);
    CImg<int> tmpImg(tmpFile.toStdString().data());

    if(!isBinary(tmpImg)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not binary image"));
        return;
    }

    tmpImg.resize(img.width(), img.height(), img.depth(), img.spectrum());
    CImg<int> result = operatorXor(img, tmpImg);
    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_AND_triggered()
{
    CImg<int> img(fileName.toStdString().data());

    if (!isBinary(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not binary image"));
        return;
    }

    QString tmpFile = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), imageFormat);

    if(!isBinary(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not binary image"));
        return;
    }

    CImg<int> tmpImg(tmpFile.toStdString().data());
    tmpImg.resize(img.width(), img.height(), img.depth(), img.spectrum());
    CImg<int> result = operatorAnd(img, tmpImg);

    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_OR_triggered()
{
    CImg<int> img(fileName.toStdString().data());

    if (!isBinary(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not binary image"));
        return;
    }

    QString tmpFile = QFileDialog::getOpenFileName(this, tr("Open File"), QDir::homePath(), imageFormat);

    if(!isBinary(img)) {
        QMessageBox::critical(this, tr("Error!"), tr("Not binary image"));
        return;
    }

    CImg<int> tmpImg(tmpFile.toStdString().data());
    tmpImg.resize(img.width(), img.height(), img.depth(), img.spectrum());
    CImg<int> result = operatorOr(img, tmpImg);

    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_FFT_triggered()
{
    CImg<double> img(fileName.toStdString().data());
    CImgList<double> fft = img.get_FFT();
    CImg<double> result = magnitude(fft);

    result.normalize(0, 255);
    result = fftshift(result);
    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_IFFT_triggered()
{
    dlgIFFT = new DialogIFFT;

    dlgIFFT->setModal(true);
    dlgIFFT->show();
    connect(dlgIFFT, SIGNAL(sendData(int)), this, SLOT(ifft(int)));
}

// shift high frequency from corner to middle
// actually, it could also use for reverse
// so fftshift & ifftshift use the same function/method
template<typename T>
CImg<T> im::fftshift(const CImg<T> &img)
{
    return img.get_shift(img.width()/2, img.height()/2, 0, 0, 2);
}

void im::on_action_Resize_triggered()
{
    dlgResize = new DialogResize;

    dlgResize->setModal(true);
    dlgResize->show();
    connect(dlgResize, SIGNAL(sendData(double, double, int)), this, SLOT(resize(double, double, int)));
}

void im::on_action_Mirror_triggered()
{
    CImg<double> img(fileName.toStdString().data());
    CImg<double> result = img.get_mirror('x');
    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_Flip_triggered()
{
    CImg<double> img(fileName.toStdString().data());
    CImg<double> result = img.get_mirror('y');
    result.save(resultFileName.toStdString().data());
    updateOutScene(resultFileName);
}

void im::on_action_Inverse_Filter_triggered()
{
    dlgInverseFilter = new DialogInverseFilter;

    dlgInverseFilter->setModal(true);
    dlgInverseFilter->show();

    connect(dlgInverseFilter,
            SIGNAL(sendData(int, int, double, int, int)),
            this,
            SLOT(invertFilter(int, int, double, int, int)));
}

template<typename T>
CImg<T> im::fft(const CImg<T> &img)
{
    // pad zeros to make width x height = 2^M x 2^N
    int width = static_cast<int>(pow(2, ceil(log2(img.width()))));
    int heigth = static_cast<int>(pow(2, ceil(log2(img.height()))));

    CImg<T> result(width, heigth, img.depth(), img.spectrum(), 0);
}

void im::on_action_Manual_Threshold_triggered()
{
    dlgManualThreshold = new DialogManualThreshold;
    dlgManualThreshold->setModal(true);
    dlgManualThreshold->show();
    connect(dlgManualThreshold, SIGNAL(sendData(int)), this, SLOT(threshold(int)));
}

// see http://blog.csdn.net/dcrmg/article/details/52216622 for details
void im::on_action_Ostu_method_triggered()
{
    CImg<double> img(fileName.toStdString().data());

    if (!isGrayscale(img)) {
        QMessageBox::critical(this, tr("Error"), tr("Non-grayscale image."));
        return;
    }

    // histogram
    CImg<double> hist = img.get_histogram(255);
    hist /= (img.width() * img.height());
    CImg<double> cum = hist.get_cumulate();

    // 前景背景所占比率
    double w0 = 0;
    double w1 = 0;
    // 前景背景的平均灰度
    double u0 = 0;
    double u1 = 0;
    // 类间方差
    double sigma = 0;
    double mu = img.mean();
    double threshold;

    for(int t = 0; t < 256; ++t) {
        w0 = cum(t);
        w1 = 1 - cum(t);
        u0 += t*hist(t);
        u1 = mu - u0;

        qDebug() << "sigma = " << w0*w1*(u0 - u1)*(u0 - u1) << "threshold = " << t << endl;
        // 类间方差
        if (sigma < w0*w1*(u0 - u1)*(u0 - u1)) {
            sigma = w0*w1*(u0 - u1)*(u0 - u1);
            threshold = t;
        }
    }

    qDebug() << "Final: sigma = " << sigma << endl;
    qDebug() << "threshold = " << threshold << endl;

    this->threshold(threshold);
}

void im::on_action_Region_Growth_triggered()
{
    dlgRegionGrowth = new DialogRegionGrowth;
    dlgRegionGrowth->setModal(false);
    dlgRegionGrowth->show();

    connect(inScene,
            SIGNAL(seedSetted(QPoint)),
            dlgRegionGrowth,
            SLOT(setSeedCoord(QPoint)));
    connect(dlgRegionGrowth,
            SIGNAL(sendData(QPoint,int)),
            this,
            SLOT(regionGrowth(QPoint,int)));

//    CImg<int> img(fileName.toStdString().data());
//    CImg<int> result(img.width(), img.height(), img.depth(), img.spectrum(), 255);
//    int T = 10;
//    int x, y;
//    QStack<QPoint> seed;
//    QPoint currentSeed;
//    seed.push(QPoint(90, 90));
//    result(90, 90) = 0;

//    while(!seed.isEmpty()) {
//        currentSeed = seed.pop();
//        x = currentSeed.x();
//        y = currentSeed.y();
//        for (int i = -1; i < 2; ++i) {
//            for (int j = -1; j < 2; ++j) {
//                if (isInsideImage(QPoint(x + i, y + j), img)) {
//                    if (result(x + i, y + j) != 0) {
//                        if (abs(img(x + i, y + j) - img(x, y)) < T) {
//                            seed.push(QPoint(x + i, y + j));
//                            result(x + i, y + j) = 0;
//                        }
//                    }
//                }
//            }
//        }
//    }

//    //qDebug() << img(100, 100) << endl;
//    //qDebug() << img(101, 100) << endl;

//    result.save("tmp.png");
//    updateOutScene("tmp.png");
}

void im::on_action_Erode_triggered()
{
    dlgErode = new DialogErode;

    dlgErode->setModal(true);;
    dlgErode->show();

    connect(dlgErode,
            SIGNAL(sendData(unsigned char[3][3])),
            this,
            SLOT(erode(unsigned char[3][3])));
}

template<typename T>
bool im::isInsideImage(const QPoint &point, const CImg<T> &img)
{
    if (point.x() < img.width() && point.x() >= 0
            && point.y() < img.height() && point.y() >= 0) {
        return true;
    } else {
        return false;
    }
}

void im::on_action_Dilate_triggered()
{
    dlgDilate = new DialogDilate;

    dlgDilate->setModal(true);
    dlgDilate->show();

    connect(dlgDilate,
            SIGNAL(sendData(unsigned char[3][3])),
            this,
            SLOT(dilate(unsigned char[3][3])));
}

void im::on_action_Opening_triggered()
{
    dlgOpening = new DialogOpening;

    dlgOpening->setModal(true);
    dlgOpening->show();

    connect(dlgOpening,
            SIGNAL(sendData(unsigned char[3][3])),
            this,
            SLOT(opening(unsigned char[3][3])));
}

void im::on_action_Closing_triggered()
{
    dlgClosing = new DialogClosing;

    dlgClosing->setModal(true);
    dlgClosing->show();

    connect(dlgClosing,
            SIGNAL(sendData(unsigned char[3][3])),
            this,
            SLOT(closing(unsigned char[3][3])));
}

void im::on_action_Ideal_High_Pass_Filter_triggered()
{
    dlgIdealHighPassFilter = new DialogIdealHighPassFilter;

    dlgIdealHighPassFilter->setModal(true);
    dlgIdealHighPassFilter->show();

    connect(dlgIdealHighPassFilter,
            SIGNAL(sendData(int)),
            this,
            SLOT(idealHighPassFilter(int)));
}

void im::on_action_Ideal_Low_Pass_Filter_triggered()
{
    dlgIdealLowPassFilter = new DialogIdealLowPassFilter;

    dlgIdealLowPassFilter->setModal(true);
    dlgIdealLowPassFilter->show();

    connect(dlgIdealLowPassFilter,
            SIGNAL(sendData(int)),
            this,
            SLOT(idealLowPassFilter(int)));
}

void im::on_action_Butterworth_Low_Pass_Filter_triggered()
{
    dlgButterworthLowPassFilter = new DialogButterworthLowPassFilter;

    dlgButterworthLowPassFilter->setModal(true);
    dlgButterworthLowPassFilter->show();

    connect(dlgButterworthLowPassFilter,
            SIGNAL(sendData(int, int)),
            this,
            SLOT(butterworthLowPassFilter(int, int)));
}

void im::on_action_Butterworth_High_Pass_Filter_triggered()
{
    dlgButterworthHighPassFilter = new DialogButterworthHighPassFilter;

    dlgButterworthHighPassFilter->setModal(true);;
    dlgButterworthHighPassFilter->show();

    connect(dlgButterworthHighPassFilter,
            SIGNAL(sendData(int, int)),
            this,
            SLOT(butterworthHighPassFilter(int, int)));
}

void im::on_action_Homomorphic_Filter_triggered()
{
    dlgHomomorphicFilter = new DialogHomomorphicFilter;

    dlgHomomorphicFilter->setModal(true);
    dlgHomomorphicFilter->show();

    connect(dlgHomomorphicFilter,
            SIGNAL(sendData(double, double, double, int)),
            this,
            SLOT(homomorphicFilter(double, double, double, int)));
}

template<typename T>
CImgList<T> im::div(const CImg<T> &img1_real, const CImg<T> &img1_imag, const CImg<T> &img2_real, const CImg<T> &img2_imag)
{
    std::complex<T> tmp1, tmp2, tmp3;
    CImgList<T> result(2, img1_real);

    cimg_forXY(result[0], x, y) {
        tmp1 = std::complex<double>(img1_real(x, y), img1_imag(x, y));
        tmp2 = std::complex<double>(img2_real(x, y), img2_imag(x, y));
        tmp3 = tmp1/tmp2;
        result[0](x, y) = tmp3.real();
        result[1](x, y) = tmp3.imag();
    }

    return result;
}

template<typename T>
CImgList<T> im::div(const CImgList<T> &img1, const CImgList<T> &img2)
{
    std::complex<T> tmp1, tmp2, tmp3;
    CImgList<T> result(img1);

    cimg_forXY(result[0], x, y) {
        tmp1 = std::complex<double>(img1[0](x, y), img1[1](x, y));
        tmp2 = std::complex<double>(img2[0](x, y), img2[1](x, y));
        tmp3 = tmp1/tmp2;
        result[0](x, y) = tmp3.real();
        result[1](x, y) = tmp3.imag();
    }

    return result;
}

void im::on_action_Motion_Blur_triggered()
{
    dlgMotionBlur = new DialogMotionBlur;

    dlgMotionBlur->setModal(true);
    dlgMotionBlur->show();

    connect(dlgMotionBlur, SIGNAL(sendData(int, int)), this, SLOT(motionBlur(int, int)));
}

// compute img1/img2 within D <= D0
template<typename T>
CImgList<T> im::div(const CImgList<T> &img1, const CImgList<T> &img2, const int &D0)
{
    CImgList<T> result(img1);
    T D;
    std::complex<T> tmp1, tmp2, tmp3;

    cimg_forXY(result[0], x, y) {
        D = sqrt((x - img1[0].width()/2)*(x - img1[0].width()/2)
                 + (y - img1[0].height()/2)*(y - img1[0].height()/2));
        if (D <= D0) {
            tmp1 = std::complex<T>(img1[0](x, y), img1[1](x, y));
            tmp2 = std::complex<T>(img2[0](x, y), img2[1](x, y));
            tmp2 = std::abs(tmp2) > std::abs(sqrt(DBL_EPSILON))? tmp2 : std::complex<T>(sqrt(DBL_EPSILON));
            tmp3 = tmp1/tmp2;
            result[0](x, y) = tmp3.real();
            result[1](x, y) = tmp3.imag();
        }
    }

    return result;
}

void im::on_action_Gaussian_Noise_triggered()
{
    dlgGaussianNoise = new DialogGaussianNoise;

    dlgGaussianNoise->setModal(true);
    dlgGaussianNoise->show();

    connect(dlgGaussianNoise, SIGNAL(sendData(double)), this, SLOT(gaussianNoise(double)));
}

template<typename T>
CImgList<T> im::fftshift(const CImgList<T> &img)
{
    CImgList<T> result(img);

    for(int i = 0; i < img.width(); ++i) {
        result[i] = fftshift(img[i]);
    }

    return result;
}

void im::on_action_Atmospheric_Circulation_Blur_triggered()
{
    dlgAtmosphericCirculation = new DialogAtmosphericCirculation;

    dlgAtmosphericCirculation->setModal(true);
    dlgAtmosphericCirculation->show();

    connect(dlgAtmosphericCirculation,
            SIGNAL(sendData(double)),
            this,
            SLOT(atmosphericCirculationBlur(double)));
}

void im::on_action_Wiener_Filter_triggered()
{
    dlgWienerFilter = new DialogWienerFilter;

    dlgWienerFilter->setModal(true);
    dlgWienerFilter->show();
    connect(dlgWienerFilter,
            SIGNAL(sendData(int, double, int, int, double)),
            this,
            SLOT(wienerFilter(int, double, int, int, double)));
}

template<typename T>
CImgList<T> im::getConj(const CImgList<T> &img)
{
    CImgList<T> result(img);
    std::complex<T> tmp;

    cimg_forXY(img[0], x, y) {
        tmp = std::complex<T>(img[0](x, y), img[1](x, y));
        result[0](x, y) = tmp.real();
        result[1](x, y) = -tmp.imag();
    }

    return result;
}

// compute FFT of psf in size width x height
// no fftshift apply
template<typename T>
CImgList<double> im::psfToOtf(const CImg<T> &img, const int &width, const int &height)
{
    // resize psf, pad zeros to size width x height
    // and return FFT result
    return img.get_resize(width, height, 1, 1, 0).get_FFT();
}

template<typename T>
CImgList<T> im::add(const CImgList<T> &img, const std::complex<T> &val)
{
    CImgList<T> result(img);

    cimg_forXY(result[0], x, y) {
        result[0](x, y) += val.real();
        result[1](x, y) += val.imag();
    }

    return result;
}

// we keep magnitude, and set phase to 1
template<typename T>
CImgList<T> im::keepMagnitude(const CImgList<T> &img)
{
    CImgList<T> result(img);
    std::complex<T> tmp1, tmp2;

    cimg_forXYZ(img[0], x, y, z) {
        tmp1 = std::complex<T>(img[0](x, y, z), img[1](x, y, z));
        tmp2 = std::polar(std::abs(tmp1), static_cast<T>(1));
        result[0](x, y, z) = tmp2.real();
        result[1](x, y, z) = tmp2.imag();
    }

    return result;
}

// we keep phase, and set magnitude to 1
template<typename T>
CImgList<T> im::keepPhase(const CImgList<T> &img)
{
    CImgList<T> result(img);
    std::complex<T> tmp1, tmp2;

    cimg_forXYZ(img[0], x, y, z) {
        tmp1 = std::complex<T>(img[0](x, y, z), img[1](x, y, z));
        tmp2 = std::polar(static_cast<T>(1), std::arg(tmp1));
        result[0](x, y, z) = tmp2.real();
        result[1](x, y, z) = tmp2.imag();
    }

    return result;
}
