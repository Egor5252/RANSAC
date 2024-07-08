#include "mainwindow.h"
#include "mygraphicsview.h"
#include "./ui_mainwindow.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <QVector>
#include <QPointF>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <functional>
#include <QUdpSocket>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    scene = new QGraphicsScene();
    ui->graphicsView->setScene(scene);
    scene->setSceneRect(0,0, ui->graphicsView->width(), ui->graphicsView->height()); // Создание сцены и установка координат как у graphicsView

    udp = new QUdpSocket(this);
    udp->bind(QHostAddress::LocalHost, 1111);        // Создание сокета
    connect(udp, SIGNAL(readyRead()), this, SLOT(ReadingData()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()  // Функция записи точек в файл
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Text"), "../", tr("Text Files (*.txt)"));

    QFile fileOut(fileName);
    if(fileOut.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream writeStream(&fileOut);
        for (auto i : vector_of_points){
            writeStream << QString::number(i.x()) + ";" + QString::number(i.y()) + "\n";
        }
        fileOut.close();
    }
}


void MainWindow::on_pushButton_2_clicked() // Чтение точек из файла и их записть в вектор
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Text"), "../", tr("Text Files (*.txt)"));

    QFile inputFile(fileName);
    if (inputFile.open(QIODevice::ReadOnly))
    {
        vector_of_points.clear();
        scene->clear();
        QTextStream in(&inputFile);
        while (!in.atEnd())
        {
            QString line = in.readLine();
            QStringList lst = line.split(";");
            QPointF Point(lst[0].toDouble(), lst[1].toDouble());
            vector_of_points.push_back(Point);
        }
        inputFile.close();

        for (auto i : vector_of_points) {
            scene->addEllipse(i.x() - 5, i.y() - 5, 10, 10, QPen(Qt::NoPen), QBrush(Qt::gray));
        }
    }
}

using namespace std;

struct ret  // Структкра хранит данне о лучшей линии
{
    QVector<QPointF> VOB; // Вектор точек inlier
    QPointF BM; // BM.x() - параметр k, BM.y() - парамерт b
};

ret fitLine(const QVector<QPointF>& points, int iterations = 100000, float threshold = 15.0) { // iterations - кол-во итераций, threshold - допуск
    srand(time(0));
    QPointF bestModel;
    int bestInliers = 0;
    QVector<QPointF> vector_of_Inliers;
    QVector<QPointF> vector_of_BestInliers;


    for (int i = 0; i < iterations; ++i) { // Реализация алгоритма RANSAC
        int idx1 = rand() % points.size();
        int idx2 = rand() % points.size();
        if (idx1 == idx2) continue;

        QPointF p1 = points[idx1];
        QPointF p2 = points[idx2];

        float m = (p2.y() - p1.y()) / (p2.x() - p1.x());
        float b = p1.y() - m * p1.x();

        int inliers = 0;
        vector_of_Inliers.clear();
        for (const auto& point : points) {
            float x = point.x();
            float y = point.y();
            float distance = fabs(m*x - y+b) / sqrt(m*m + 1);
            if (distance < threshold) {
                ++inliers;
                vector_of_Inliers.push_back(QPointF(point.x(), point.y()));
            }
        }

        if (inliers > bestInliers) {
            bestInliers = inliers;
            vector_of_BestInliers = vector_of_Inliers;
            bestModel = QPointF(m, b);
        }
    }


    ret retir;
    retir.VOB = vector_of_BestInliers;
    retir.BM = bestModel;

    return retir;
}

struct ransacresult {
    QVector<QPointF> inliers;
    QPointF model;
    int inliersCount;
};

ransacresult runRansacIteration(const QVector<QPointF>& points, float threshold) { // Реализация распределённого вычисления RANSAC
    // Выбор двух случайных точек
    int idx1 = rand() % points.size();
    int idx2 = rand() % points.size();
    if (idx1 == idx2) return { {}, {}, 0 };

    QPointF p1 = points[idx1];
    QPointF p2 = points[idx2];

    // Расчет модели прямой (y = mx + b)
    float m = (p2.y() - p1.y()) / (p2.x() - p1.x());
    float b = p1.y() - m * p1.x();

    QVector<QPointF> inliers;
    int inliersCount = 0;

    // Проверка, какие точки являются "inliers"
    for (const auto& point : points) {
        float x = point.x();
        float y = point.y();
        float distance = fabs(m * x - y + b) / sqrt(m * m + 1);
        if (distance < threshold) {
            ++inliersCount;
            inliers.push_back(point);
        }
    }

    return {inliers, QPointF(m, b), inliersCount};
}

ret fitLine_paralel(const QVector<QPointF>& points, int iterations = 100000, float threshold = 15.0) { // Реализация распределённого вычисления RANSAC
    srand(time(0));

    // Создаем вектор задач для параллельного выполнения
    QVector<QFuture<ransacresult>> futures;
    QThreadPool pool;
    for (int i = 0; i < iterations; ++i) {
        futures.push_back(QtConcurrent::run(&pool, runRansacIteration, points, threshold));
    }

    ransacresult bestResult;
    bestResult.inliersCount = 0;

    for (auto& future : futures) {
        future.waitForFinished();
        ransacresult result = future.result();
        if (result.inliersCount > bestResult.inliersCount) {
            bestResult = result;
        }
    }

    ret retir;
    retir.VOB = bestResult.inliers;
    retir.BM = bestResult.model;

    return retir;
}
QPointF line;
void MainWindow::on_pushButton_3_clicked() // Действие по кнопке Вычислить
{
    qint64 start_time = QDateTime::currentMSecsSinceEpoch(); // Получение системного времени начала вычисления

    ret output;
    int iter = ui->lineEdit_2->text().toInt();
    if(ui->checkBox->isChecked()) {
        output = fitLine_paralel(vector_of_points, iter);
    }
    else {
        output = fitLine(vector_of_points, iter);
    }
    line = output.BM;
    QVector<QPointF> Inliners = output.VOB;
    ui->lineEdit->setText("y = " + QString::number(line.x()) + " * x " + " + " + QString::number(line.y()));
    scene->clear();
    QPointF dots[2];
    qreal x = -1;
    qreal y = line.x() * x + line.y();
    dots[0].rx() = x; dots[0].ry() = y;
    x = ui->graphicsView->width()+1; y = line.x() * x + line.y();
    dots[1].rx() = x; dots[1].ry() = y;

    scene->addLine(QLineF(dots[0], dots[1]), QPen(Qt::blue));

    for (auto i : vector_of_points) {
        scene->addEllipse(i.x() - 5, i.y() - 5, 10, 10, QPen(Qt::NoPen), QBrush(Qt::red));
    }

    for (auto i : Inliners) {
        scene->addEllipse(i.x() - 5, i.y() - 5, 10, 10, QPen(Qt::NoPen), QBrush(Qt::blue));
    }

    qint64 end_time = QDateTime::currentMSecsSinceEpoch();
    QMessageBox msgBox;
    msgBox.setText("Прошло " + QString::number(end_time - start_time) + "мс"); // Получение системного времени конца вычисления и отображение результата
    msgBox.exec();
}


void MainWindow::on_pushButton_4_clicked() // Отправить уравнение
{
    QString sdata = QString::number(line.x())+";"+QString::number(line.y());
    udp->writeDatagram(sdata.toUtf8(), QHostAddress::LocalHost, 1234);
}

