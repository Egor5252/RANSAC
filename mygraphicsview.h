#ifndef MYGRAPHICSVIEW_H
#define MYGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QMouseEvent>
#include <QVector>
#include <QGraphicsItem>
#include <QTransform>

QVector<QPointF> vector_of_points;

class MyGraphicsView: public QGraphicsView {
public:
    MyGraphicsView(QWidget* parent = nullptr): QGraphicsView(parent) {}

protected:
    void mousePressEvent(QMouseEvent* event) override {  // Созжание действия по клику левой кнопки мыши для создания/удаления точек
        if (event->button() == Qt::LeftButton) {
            QPointF scenePoint = mapToScene(event->pos());

            if (!scene()->itemAt(scenePoint, QTransform())) {  // Если нажатие мыши было не по точке. Добавление точки
                scene()->addEllipse(scenePoint.x() - 5, scenePoint.y() - 5, 10, 10, QPen(Qt::NoPen), QBrush(Qt::gray));
                vector_of_points.push_back(scenePoint);
            } else {
                delete scene()->itemAt(scenePoint, QTransform()); // Если нажатие было по точке. Стереть точку и удалить её из вектора
                qreal min_dist = 1000;
                QPointF for_delete;
                for (int i = 0; i < vector_of_points.size(); ++i) {
                    qreal dist = sqrt(pow(vector_of_points[i].x() - scenePoint.x(),2) + pow(vector_of_points[i].y()-scenePoint.y(),2));
                    if (dist < min_dist) {
                        min_dist = dist;
                        for_delete = vector_of_points[i];
                    }
                }
                erase(vector_of_points, for_delete);
            }
        }
    }
};
#endif // MYGRAPHICSVIEW_H
