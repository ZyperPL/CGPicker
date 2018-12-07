#include <QtWidgets>
#include <QSizePolicy>

#include "mainwindow.hpp"
#include "renderarea.hpp"

#define BUTTON_SIZE 128

std::map<ShapeType, QString> shapeNames = { 
  { ShapeType::Polygon, "Polygon" }, 
  { ShapeType::Circle, "Circle" }, 
  { ShapeType::Rectangle, "Rectangle" } 
};


ShapeItem::ShapeItem(Shape *shape) : QListWidgetItem()
{
  this->shape = shape;
  setText(shapeNames.at(shape->type));
}

MainWindow::MainWindow()
{
  createMenus();
 
  QWidget *mainWidget = new QWidget;
  setCentralWidget(mainWidget);
  allLayout = new QHBoxLayout;
  mainWidget->setLayout(allLayout);

  auto createButton = [=](const char *icon, const char *text, void(MainWindow::*method)(BARG), BARG arg) -> QWidget*
  {
    QToolButton* b = new QToolButton(this);
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setIcon(QIcon(icon));
    b->setIconSize(QSize(BUTTON_SIZE/2, BUTTON_SIZE/2));
    b->setText(text);
    //b->setFixedSize(QSize(BUTTON_SIZE, BUTTON_SIZE));
    //b->setMinimumSize(QSize(BUTTON_SIZE, BUTTON_SIZE)); 
    
    //qDebug("arg: %p\n", arg);
    connect(b, &QToolButton::pressed, this, [=]{ (this->*method)(arg); });
    return b;
  };

  // buttons bar
  QVBoxLayout *buttonsLayout = new QVBoxLayout;
  buttonsLayout->addWidget(createButton(":/res/icons/pointer.svg", "Select", &MainWindow::selected, BARG(ToolType::Select) ));
  buttonsLayout->addWidget(createButton(":/res/icons/polygon.svg", "Polygon", &MainWindow::selected, BARG(ToolType::Polygon)));
  buttonsLayout->addWidget(createButton(":/res/icons/circle.svg", "Circle", &MainWindow::selected, BARG(ToolType::Circle)));
  buttonsLayout->addWidget(createButton(":/res/icons/rectangle.svg", "Rectangle", &MainWindow::selected, BARG(ToolType::Rectangle) ));
  buttonsLayout->addWidget(createButton(":/res/icons/colors.svg", "Color", &MainWindow::selected, BARG(ToolType::Color) ));
  buttonsLayout->addStretch();

  allLayout->addLayout(buttonsLayout);

  // central view
  this->area = new RenderArea(this);
  area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  allLayout->addWidget(area);
   
  // shapes list
  this->shapesList = new QListWidget(this);
  this->shapesList->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
  this->shapesList->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(shapesList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));
  connect(shapesList, SIGNAL(itemPressed(QListWidgetItem*)), this, SLOT(shapeSelected(QListWidgetItem*)) );
  allLayout->addWidget(shapesList);


  //allLayout->setSizeConstraint(QLayout::SetFixedSize);
  
  // statusbar
  setStatusBar(new QStatusBar(this));
  statusBar()->showMessage(tr("Loaded."), 10);
}

void MainWindow::createMenus()
{
  #define NONBOLD false
  #define BOLD    true

  auto createAction = [this](const char *text, void(MainWindow::*method)(), bool bold = false) -> QAction*
  {
    QAction *action = new QAction(tr(text), this);
    if (bold) {
      auto boldFont = action->font();
      boldFont.setBold(true);
      action->setFont(boldFont);
    }

    connect(action, &QAction::triggered, this, method);

    return action;
  };

  QMenu *projectMenu = menuBar()->addMenu(tr("&Project"));
  projectMenu->addAction(createAction("&New", &MainWindow::newProject));
  //TODO: add short cuts
  projectMenu->addAction(createAction("&Load", &MainWindow::loadProject, BOLD));
  projectMenu->addAction(createAction("&Save", &MainWindow::saveProject));
  projectMenu->addAction(createAction("&Exit", &MainWindow::exit));


  QMenu *dataMenu = menuBar()->addMenu(tr("&Data"));
  dataMenu->addAction(createAction("&Load image", &MainWindow::loadImage));

  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(createAction("&About", &MainWindow::about));
}

void MainWindow::newProject()
{
  allLayout->removeWidget(this->area);
  delete this->area;
  this->area = new RenderArea(this);
  area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  allLayout->insertWidget(1, this->area);

  shapesList->clear();
}

void MainWindow::loadProject()
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), ".", tr("All files (*);;Project files (*.gk2)"));

  bool ret = loadProjectFile(fileName);

  if (!ret)
  {
    QMessageBox::critical(this, tr("Cannot open file"),
            tr("File cannot be opened! "));
    return;
  }
  qDebug("Project loaded.");
}

bool MainWindow::loadProjectFile(QString fileName)
{
  QFile file(fileName);
  if (!file.open(QFile::ReadOnly)) {
    qDebug("Cannot read file %s!", fileName.toStdString().c_str() );
    return false;
  }
    
  QByteArray b = file.readAll();
  file.close();
  
  qDebug("Data size: %d.\n", b.size());

  if (b[0] != 'G' || b[1] != 'K' || b[2] != '2') 
  {
    qDebug("Magic not found!");
    return false;
  }

  size_t fileNameSize = b.at(3);
  QString imageName = b.data()+sizeof(char)*3+sizeof(int8_t);

  if (imageName.size() + 1 != fileNameSize)
  {
    qDebug("Warning: Wrong file name sizes!");
    return false;
  }

  this->newProject();
  this->area->loadImage(imageName);

  // MAGICSTR + SIZEOF(FILENAME) + FILENAME = DATA_OFFSET 
  size_t p = sizeof(int8_t) + sizeof(char)*3 + fileNameSize*sizeof(char);
  
  while (p < b.size())
  {
    Shape *shape;
    size_t dp = 0;
    std::tie(shape, dp) = RenderArea::deserializeShape((int8_t*)(b.data()+p), b.size()-p);

    p += dp;
    
    if (shape->type != ShapeType::Polygon)
    {
      int vx = shape->position.x + shape->size.x/2;
      int vy = shape->position.y + shape->size.y/2;
      shape->vertices.append(QPoint(vx, vy));
    }
    area->addShape(shape);
    this->addedShape(shape);
  }

  return true;
}

void MainWindow::saveProject()
{
  auto shapes = area->getShapes();
 
  QByteArray b;
  b.append("GK2"); // add magic
  b.append(area->fileName.size()+1); //TODO: Allow file name > 255bytes
  for (const auto c : area->fileName) {
    b.append(c);
  }
  b.append((char)0);
  
  for (const auto s : shapes)
  {
    int8_t *data;
    size_t size;

    std::tie(data, size) = RenderArea::serializeShape(s);
    for (size_t i = 0; i < size; i++) {
      auto c = data[i];
      b.append(c);
    }
    
    delete[] data;
  }

  //qDebug("Data: %lu bytes (capacity=%lu).", theData.size(), theData.capacity());
  //for (auto const d : theData) { qDebug("%4x (%c) ", d, (char)d); }

  QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                           ".",
                           tr("Project files (*.gk2)"));

  QFile file(fileName);
  if (file.open(QFile::WriteOnly)) {
    file.write(b);
  }
  file.close();

  qDebug("Saved %d bytes to %s.", b.size(), fileName.toStdString().c_str());
}

void MainWindow::addedShape(Shape *shape)
{
  if (!shape) return;

  shapesList->addItem(new ShapeItem(shape));
}

void MainWindow::deleteItem()
{
  for (int i = 0; i < shapesList->selectedItems().size(); ++i) {
      QListWidgetItem *item = shapesList->takeItem(shapesList->currentRow());
      auto shapeItem = dynamic_cast<ShapeItem*>(item);
      area->deleteShape(shapeItem->shape);
      delete item;
  }
}
      
void MainWindow::shapeSelected(QListWidgetItem *item)
{
  qDebug("Item %p selected.", item);
  auto shapeItem = dynamic_cast<ShapeItem*>(item);
  area->setSelected(shapeItem->shape);
}
  
void MainWindow::showContextMenu(const QPoint &pos)
{
  //qDebug("%d %d\n", pos.x(), pos.y());
  
  QPoint globalPos = shapesList->mapToGlobal(pos);
  QMenu menu;
  menu.addAction("Delete", this, SLOT(deleteItem()));
  menu.exec(globalPos);
}

void MainWindow::loadImage()
{
  auto imageName = QFileDialog::getOpenFileName(this,
    tr("Open Data Image"), "./images/", tr("Image Files (*.png *.jpg *.bmp)"));

  qDebug("Image file name: %s\n", imageName.toStdString().c_str());
  
  if (!area->loadImage(imageName))
  {
    QMessageBox::critical(this, tr("Error"),
            tr("Image \"<i>") + imageName + tr("</i>\" cannot be loaded!"));
  }
  area->repaint();

  if (!area->getImage()->isNull())
  {
    statusBar()->showMessage(tr("Image loaded."));
  }
}

void MainWindow::exit()
{
  close();
}

void MainWindow::about()
{
  QMessageBox::about(this, tr("About Menu"),
          tr("The <b>GK2 Application</b> is made for Computer Graphics class "
              "by <i>Kacper Zybała</i>."));
}

void MainWindow::selected(BARG arg)
{
  ToolType type = arg.data.toolType;
  QString msg = QString::asprintf("Tool %d. selected.", type);
  statusBar()->showMessage(msg);

  if ((type > ToolType::NONE && type < ToolType::Color) || type == ToolType::Select)
    area->setTool(type);  
  else if (type == ToolType::Color)
    area->setColor(QColorDialog::getColor());
}
