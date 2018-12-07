#include "renderarea.hpp"
#include "mainwindow.hpp"

#include <QtWidgets>

RenderArea::RenderArea(QWidget *parent) : QWidget(parent)
{
  myParent = parent;

  setMouseTracking(true);
}

RenderArea::~RenderArea()
{
  delete this->currentShape;
  for (auto &obj : this->shapes)
    delete obj;

  delete this->image;
}

bool RenderArea::loadImage(const QString &fileName)
{
  if (image) delete image;
  this->fileName = fileName.toStdString();

  image = new QImage(fileName);
  return !image->isNull();
}
 
void RenderArea::mousePressEvent(QMouseEvent *event)
{
  (void)(event);
  if (!image) return;
  if (image->isNull()) return;
  if (tool != ToolType::Polygon && currentShape) return; //already drawing

  if (event->buttons() & Qt::LeftButton)
  {
    auto clickPos = toImageSpace(event->pos());
    
    // if new shape is not being drawn
    if (!currentShape) {
      // set start position of new shape
      shapeCreationPosition = clickPos;
    } 

    switch (tool)
    {
      case ToolType::Circle:
        currentShape = new Shape(ShapeType::Circle, shapeCreationPosition, Vec2(0,0), color);
        currentShape->vertices.push_back(QPoint(shapeCreationPosition)); // add move anchor
        break;
      case ToolType::Rectangle:
        currentShape = new Shape(ShapeType::Rectangle, shapeCreationPosition, Vec2(0,0), color);
        currentShape->vertices.push_back(QPoint(shapeCreationPosition)); // add move anchor
        break;
      case ToolType::Polygon:
        if (!currentShape)
        {
          // create new polygon shape if it is not being drawn
          currentShape = new Shape(ShapeType::Polygon, shapeCreationPosition, color);
        } 

        break;
      case ToolType::Select:
        if (selectedShape)
        {
          auto *s = selectedShape;
          auto dist = (clickPos - QPoint(s->position.x, s->position.y)).manhattanLength();
          if (dist <= POLYGON_END_RADIUS)
          {
            // selected shape anchor for moving
            selectedOrigin = true;
          } else
          {
            for (auto i = 0; i < s->vertices.size(); i++)
            {
              const auto &v = s->vertices.at(i);
              auto vdist = (clickPos - v).manhattanLength();
              qDebug("%d: %d", i, vdist);
              if (vdist <= VERTEX_SIZE)
              {
                // selected shape vertex (or anchor for non-polygon shapes)
                selectedVertex = i;
                qDebug("Selected vertex %d.\n", i);
                break;
              }

            }
          }
        }
        break;
      default:
        qDebug("Tool %d not supported!\n", tool);
    }
  }
}

void RenderArea::mouseMoveEvent(QMouseEvent *event)
{
  if (!currentShape && !selectedShape) return;
  if (tool == ToolType::Polygon) return;

  auto startPos = shapeCreationPosition;
  static auto lastMovePos = toImageSpace(event->pos());
  auto endPos = toImageSpace(event->pos());
  auto diff = (endPos - startPos);
  
  auto realSize = realImageSize();
  int iw = realSize.x();
  int ih = realSize.y();
  double ratioX = (double)(iw) / (double)(image->width());
  double ratioY = (double)(ih) / (double)(image->height());

  if (tool == ToolType::Select)
  {
    if (selectedOrigin)
    {
      selectedShape->position.x = endPos.x();
      selectedShape->position.y = endPos.y();
      auto mdiff = (endPos - lastMovePos);
      for (auto i = 0; i < selectedShape->vertices.size(); i++)
      {
        selectedShape->vertices.replace(i, selectedShape->vertices.at(i) + mdiff);
      }
    } else
    if (selectedVertex >= 0)
    {
      //qDebug("Moving vertex %d to %d;%d", selectedVertex, endPos.x(), endPos.y());
      selectedShape->vertices.replace(selectedVertex, endPos);

      // change size based on some shapes
      if (selectedShape->type != ShapeType::Polygon)
      {
        auto vdiff = (endPos - lastMovePos);
        selectedShape->size.x += vdiff.x()*2;
        selectedShape->size.y += vdiff.y()*2;
      }
    }
  }


  QVector2D size(diff.x(), diff.y() ); 

  if (currentShape)
  {
    currentShape->size = size*2; 

    // always place extra vertex for some shapes
    if (currentShape->type != ShapeType::Polygon)
    {
      currentShape->vertices.replace(0, endPos);
    }
  }

  repaint();

  lastMovePos = toImageSpace(event->pos());
}

void RenderArea::mouseReleaseEvent(QMouseEvent *event)
{
  if (tool == ToolType::NONE) return;

  if (currentShape)
  {
    auto startPos = shapeCreationPosition;
    auto endPos = toImageSpace(event->pos());

    auto diff = (endPos - startPos);
    QVector2D size((diff.x()), (diff.y())); 
    auto dist = diff.manhattanLength();
    qDebug("Distance to start point: %d\n", dist);

    currentShape->size = size*2;

    bool polygonEnd = true;

    if (tool == ToolType::Polygon)
    {
      if (dist > POLYGON_END_RADIUS || currentShape->vertices.size() <= 0) 
      {
        if (currentShape->vertices.size() <= 0)
        {
          // if polygon does not have vertices we set polygon anchor position to first vertex position
          shapeCreationPosition = toImageSpace(event->pos());
        }
        polygonEnd = false;
        // place new vertex
        currentShape->vertices.push_back(toImageSpace(event->pos()));
      } else
      {
        // creation is ended when click is outside radius
        polygonEnd = true;
      }
          
      qDebug("Current shape vertices: %d\n", currentShape->vertices.size());
    }

    if (polygonEnd)
    {
      // if mouse didn't move during mouse move
      if (dist > 1 || (tool == ToolType::Polygon))
      {
        // add shape to rendering list
        this->shapes.push_back(currentShape);
        qDebug("New shape is added. List: %zu elements.\n", shapes.size());
        // add shape to shapes list
        dynamic_cast<MainWindow*>(myParent)->addedShape(currentShape);
      } else
      {
        // if shape is too small delete it
        delete currentShape;
      }

      currentShape = nullptr;
    } 

  }

  // if we did not moved vertex or shape origin
  if (selectedVertex < 0 && !selectedOrigin)
  {
    // unselect shape
    selectedShape = nullptr;
  }
  selectedVertex = -1;
  selectedOrigin = false;

  repaint();
}

void RenderArea::paintEvent(QPaintEvent *event)
{
  if (!image) return;
  if (image->isNull()) 
  {
    qDebug("Image is not loaded!");
    return;
  }

  QPainter painter(this);

  int w = width();
  int h = height();

  QImage img = image->scaled(w, h, Qt::KeepAspectRatio);
  int iw = img.width();
  int ih = img.height();
  painter.drawImage(w/2 - iw/2, h/2 - ih/2,img);

  for (const auto &shape : shapes)
  {
    drawShape(shape, painter);
  }
  drawShape(currentShape, painter);
}

void RenderArea::drawShape(Shape *shape, QPainter &painter)
{
  if (!shape) return;

  QPoint pos = toWidgetSpace(shape->position.x, shape->position.y);
  
  auto realSize = realImageSize();
  int iw = realSize.x();
  int ih = realSize.y();
  double ratioX = (double)(iw) / (double)(image->width());
  double ratioY = (double)(ih) / (double)(image->height());
  auto size = QPoint(shape->size.x * ratioX ,
                     shape->size.y * ratioY);
  
  if (currentShape == shape)
  {
    // draw anchor point
    painter.setBrush(Qt::black);
    QPoint shapePos = toWidgetSpace(shapeCreationPosition);
    painter.drawEllipse(shapePos.x()-POLYGON_END_RADIUS/2*ratioX, shapePos.y()-POLYGON_END_RADIUS/2*ratioX, 
                        POLYGON_END_RADIUS*ratioX, POLYGON_END_RADIUS*ratioX);
    painter.setBrush(Qt::transparent);
    QBrush brush;
  } else
  {
    if (tool == ToolType::Select)
    {
      if (shape == selectedShape) 
      {
        // draw anchor point
        painter.setBrush(Qt::black);
        painter.drawEllipse(pos.x()-POLYGON_END_RADIUS/2*ratioX, pos.y()-POLYGON_END_RADIUS/2*ratioX, 
                            POLYGON_END_RADIUS*ratioX, POLYGON_END_RADIUS*ratioX);
        painter.setBrush(Qt::transparent);

        // draw mask
        switch (shape->type)
        {
          case ShapeType::Circle:
            painter.drawRect(QRect(pos.x()-size.x()/2, pos.y()-size.y()/2, size.x(), size.y()));
            break;
          case ShapeType::Rectangle:
            painter.drawRect(QRect(pos.x(), pos.y(), size.x()/2, size.y()/2));
            break;
          case ShapeType::Polygon:
            break;
          default:
            break;
        }

        painter.setBrush(Qt::black);
        for (const auto &p : shape->vertices)
        {
          auto ppos = toWidgetSpace(p.x(), p.y());
          auto psize = VERTEX_SIZE*ratioX;
          painter.drawEllipse(ppos.x()-psize/2, ppos.y()-psize/2, psize, psize);
        }
        painter.setBrush(Qt::transparent);
      }
    }
  }

  // draw actual shape
  painter.setPen(QColor(shape->color.r, shape->color.g, shape->color.b));
  switch (shape->type)
  {
    case ShapeType::Circle:
      painter.drawEllipse(QRect(pos.x()-size.x()/2, pos.y()-size.y()/2, size.x(), size.y()));
      break;
    case ShapeType::Rectangle:
      painter.drawRect(QRect(pos.x(), pos.y(), size.x()/2, size.y()/2));
      break;
    case ShapeType::Polygon:
      {
        QVector<QPoint> points;
        for (const auto &p : shape->vertices){
          points.push_back(toWidgetSpace(p));
        }
        if (currentShape == shape)
          painter.drawPolyline(points); // incomplete polygon
        else
          painter.drawPolygon(points); // complete polygon
      }
      break;

    default:
      qDebug("Shape %d. not supported!\n", shape->type);
  }
}
  
QPoint RenderArea::toWidgetSpace(QPoint point)
{
  int px = point.x();
  int py = point.y();

  int w = width(), h = height();

  auto realSize = realImageSize();
  int iw = realSize.x();
  int ih = realSize.y();

  double ratioX = (double)(iw) / (double)(image->width());
  double ratioY = (double)(ih) / (double)(image->height());

  int sx = w/2 - iw/2 + px*ratioX;
  int sy = h/2 - ih/2 + py*ratioY;

  return QPoint(sx, sy);
}

QPoint RenderArea::toImageSpace(QPoint point)
{
  int mx = point.x(), my = point.y();
  double imageRatio = (double)image->width() / (double)image->height();

  int w = width(), h = height();
  auto realSize = realImageSize();
  int iw = realSize.x();
  int ih = realSize.y();

  int ox = w/2 - iw/2, oy = h/2 - ih/2;
  mx -= ox; my -= oy;

  mx *= (double)(image->width()) / (double)(iw);
  my *= (double)(image->height()) / (double)(ih);

  return QPoint(mx, my);
}
