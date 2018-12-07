#include <QtGui>
#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QString>
#include <QMainWindow>

#include "mainwindow.hpp"
#include "renderarea.hpp"

int main(int argc, char *argv[]) 
{
  QApplication app(argc, argv);
  MainWindow window;

  if (argc >= 2) {
    if (!window.loadProjectFile(argv[1]))
    if (!window.getArea()->loadImage(argv[1]))
    {
      qDebug("File cannot be loaded."); 
      QString message;
      message = message.sprintf("File <i>%s</i> cannot be loaded!", argv[1]);
      QMessageBox::warning(&window, QString("Warning"), message);
           
    }
  }

  window.show();
  return app.exec();
}
