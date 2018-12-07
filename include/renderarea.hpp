#pragma once

#include <QBrush>
#include <QPen>
#include <QPixmap>
#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QVector2D>
#include <QVector>

#include <vector>
#include <map>

#define POLYGON_END_RADIUS 40
#define VERTEX_SIZE 20

class MainWindow;

enum class ToolType
{
  NONE, Polygon, Circle, Rectangle, Color, Select, TOOLTYPE_MAX
};

enum class ShapeType
{
  Circle, Rectangle, Line, Polygon, SHAPETYPE_MAX 
};

struct Color
{
  int r;
  int g;
  int b;

  Color(QColor c) : r{c.red()}, g{c.green()}, b{c.blue()} {}
  Color(int r, int g, int b) : r{r}, g{g}, b{b} {}
};

struct Vec2
{
  int32_t x;
  int32_t y;

  Vec2(QPoint p) : x{p.x()}, y{p.y()} {}
  Vec2(QVector2D v) : Vec2{v.toPoint()} {}
  Vec2(int x, int y) : x{x}, y{y} {}
};

struct Shape
{
  ShapeType type{ShapeType::SHAPETYPE_MAX};
  Vec2 position{0,0};
  Vec2 size{0,0};
  Color color{0,0,0};
  QVector<QPoint> vertices;

  Shape(ShapeType t, Vec2 p, Vec2 s, Color c)
    : type{t}, position{p}, size{s}, color{c}
  {};
  
  Shape(ShapeType t, Vec2 p, Color c)
    : type{t}, position{p}, size{0,0}, color{c}
  {};

  Shape() {}
};

class RenderArea : public QWidget
{
  Q_OBJECT

  public:
      RenderArea(QWidget *parent);
      ~RenderArea();
      bool loadImage(const QString &fileName);
      inline QImage *getImage() { return this->image; }
      void paintEvent(QPaintEvent *event);
      void mouseMoveEvent(QMouseEvent *event);
      void mousePressEvent(QMouseEvent *event);
      void mouseReleaseEvent(QMouseEvent *event);
 
      inline void setTool(ToolType type) { tool = type; }
      inline auto getShapes() { return shapes; }
      inline void addShape(Shape *shape) { shapes.push_back(shape); }

      inline QPoint toImageSpace(int x, int y) { return toImageSpace(QPoint(x,y)); }
      QPoint toImageSpace(QPoint point);

      QPoint toWidgetSpace(QPoint point);
      inline QPoint toWidgetSpace(int x, int y) { return toWidgetSpace(QPoint(x, y)); }
  
      inline void setColor(QColor c) { color = c; }
      inline void setSelected(Shape *s) { selectedShape = s; tool = ToolType::Select; repaint(); }

      inline void deleteShape(Shape *shape)
      {
        shapes.erase(std::remove(shapes.begin(), shapes.end(), shape), shapes.end());
        delete shape;
        repaint();
        this->currentShape = nullptr;
        this->selectedShape = nullptr;
      }

      static inline std::pair<int8_t*,size_t> serializeShape(Shape *shape)
      {
        /*
         * u8[type] i32[pos.x] i32[pos.y] i32[size.x] i32[size.y] i[r] i[g] i[b] size_t[vertices.size] i32[v1.x] i32[v1.y] ...
         */
        int8_t type = (int8_t)shape->type;
        size_t vertexDataSize = sizeof(int32_t) * 2;
        size_t verticesSize = (size_t)(shape->vertices.size());
        if (shape->type != ShapeType::Polygon) verticesSize = 0;

        size_t dataSize = sizeof(uint8_t) + sizeof(int32_t)*4 + sizeof(int)*3 + sizeof(size_t) + vertexDataSize*verticesSize;

        int8_t *data = new int8_t[dataSize];

        size_t p = 0;
        auto serial = [data, &p](const void *src, size_t n)
        {
          memcpy(data+p, src, n);
          p += n;
        };
        serial(&type, sizeof(int8_t));
        printf("%d\n", type);
        serial(&shape->position.x, sizeof(int32_t));
        serial(&shape->position.y, sizeof(int32_t));
        serial(&shape->size.x, sizeof(int32_t));
        serial(&shape->size.y, sizeof(int32_t));
        serial(&shape->color.r, sizeof(int));
        serial(&shape->color.g, sizeof(int));
        serial(&shape->color.b, sizeof(int));
        serial(&verticesSize, sizeof(size_t));
        if (verticesSize > 0)
        {
          for (const auto &v : shape->vertices)
          {
            int32_t x = v.x();
            int32_t y = v.y();
            serial(&x, sizeof(int32_t));
            serial(&y, sizeof(int32_t));
          }
        }

        return std::make_pair(data, dataSize);
      }

      static inline std::pair<Shape*,size_t> deserializeShape(int8_t *data, size_t dsize)
      {
        size_t p = 0;
        Shape *shape = new Shape();
        /*
         * u8[type] i32[pos.x] i32[pos.y] i32[size.x] i32[size.y] i[r] i[g] i[b] size_t[vertices.size] i32[v1.x] i32[v1.y] ...
         */

        auto deserial = [data, &p](void *dest, size_t size)
        {
          memcpy(dest, data+p, size);
          p += size;
        };

        deserial(&shape->type, sizeof(int8_t));
        deserial(&shape->position.x, sizeof(int32_t));
        deserial(&shape->position.y, sizeof(int32_t));
        deserial(&shape->size.x, sizeof(int32_t));
        deserial(&shape->size.y, sizeof(int32_t));
        deserial(&shape->color.r, sizeof(int));
        deserial(&shape->color.g, sizeof(int));
        deserial(&shape->color.b, sizeof(int));

        size_t verticesSize;
        deserial(&verticesSize, sizeof(size_t));

        if (p > dsize)
        {
          qDebug("Data pointer outside data! %lu > %lu\n", p, dsize);
        }

        for (size_t i = 0; i < verticesSize; i++)
        {
          int32_t x, y;
          deserial(&x, sizeof(int32_t));
          deserial(&y, sizeof(int32_t));
          shape->vertices.push_back(QPoint(x, y));
        }
        
        return {shape, p};
      }

      std::string fileName;
  private:
      QWidget *myParent;
      QImage *image{nullptr};
      ToolType tool{ToolType::NONE};
      QColor color;

      std::vector<Shape*> shapes;
      Shape *currentShape{nullptr};
      Shape *selectedShape{nullptr};
      int selectedVertex{-1};
      bool selectedOrigin{false};

      void drawShape(Shape *shape, QPainter &painter);
      QPoint shapeCreationPosition;

      inline QPoint realImageSize()
      {
        double imageRatio = (double)image->width() / (double)image->height();

        int w = width(), h = height();
        int iw = w, ih = h;
        iw = ih*imageRatio;
        if (w < iw) 
        {
          iw = w;
          ih = w*(1.0/imageRatio);
        }

        //qDebug("%d %d (%d %d) [%d %d] %f\n", w, h, iw, ih, image->width(), image->height(), imageRatio);
        return QPoint(iw, ih);
      }

};
