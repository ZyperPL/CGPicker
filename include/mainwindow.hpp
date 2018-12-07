#pragma once

#include <QMainWindow>
#include <QListWidgetItem>

class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QListWidget;
class QHBoxLayout;

class RenderArea;
struct Shape;

enum class ToolType;

struct ButtonArgument
{
  union 
  {
    ToolType toolType;
  } data;
  ButtonArgument(ToolType t) : data{t} {};
};

typedef struct ButtonArgument BARG;

class ShapeItem : public QListWidgetItem
{
  public:
  ShapeItem(Shape *shape);

  Shape *shape{nullptr};

};

class MainWindow : public QMainWindow
{
  Q_OBJECT

  public:
    MainWindow();
    void createMenus();

    // menu bar
    void newProject();
    void loadProject();
    void saveProject();
    void loadImage();
    void exit();
    void about();

    // buttons
    void selected(BARG arg);

    public slots:
      void shapeSelected(QListWidgetItem *item);
      void showContextMenu(const QPoint &pos);
      void deleteItem();

    void addedShape(Shape *shape);
    bool loadProjectFile(QString fileName);

    inline RenderArea *getArea() { return area; }

  private:
    RenderArea *area;
    QListWidget *shapesList;
    QHBoxLayout *allLayout;
};
