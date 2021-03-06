#include <cmath>
#include <fstream>
#include <iostream>

#include <QPainter>
#include <QString>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QFileDialog>

#include "emuwindow.hpp"

using namespace std;

ifstream::pos_type filesize(const char* filename)
{
    ifstream in(filename, ifstream::ate | ifstream::binary);
    return in.tellg();
}

EmuWindow::EmuWindow(QWidget *parent) : QMainWindow(parent)
{
    old_frametime = chrono::system_clock::now();
    old_update_time = chrono::system_clock::now();
    framerate_avg = 0.0;

    QWidget* widget = new QWidget;
    setCentralWidget(widget);

    QWidget* topFiller = new QWidget;
    topFiller->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget *bottomFiller = new QWidget;
    bottomFiller->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(5);
    layout->addWidget(topFiller);
    layout->addWidget(bottomFiller);
    widget->setLayout(layout);

    create_menu();
}

int EmuWindow::init(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Args: [BIOS] (Optional)[ELF/ISO]\n");
        return 1;
    }

    char* bios_name = argv[1];

    char* file_name;
    if (argc == 4)
        file_name = argv[2];
    else if (argc == 3 && strcmp(argv[2], "-skip") != 0)
        file_name = argv[2];
    else
        file_name = nullptr;

    bool skip_BIOS = false;
    //Flag parsing - to be reworked
    if (argc >= 3)
    {
        if (argc == 3)
        {
            if (strcmp(argv[2], "-skip") == 0)
                skip_BIOS = true;
        } 
        else if (argc == 4)
        {
            if (strcmp(argv[3], "-skip") == 0)
                skip_BIOS = true;
        }
    }

    ifstream BIOS_file(bios_name, ios::binary | ios::in);
    if (!BIOS_file.is_open())
    {
        printf("Failed to load PS2 BIOS from %s\n", bios_name);
        return 1;
    }
    printf("Loaded PS2 BIOS.\n");
    uint8_t* BIOS = new uint8_t[1024 * 1024 * 4];
    BIOS_file.read((char*)BIOS, 1024 * 1024 * 4);
    BIOS_file.close();
    e.load_BIOS(BIOS);
    delete[] BIOS;
    BIOS = nullptr;

    if (file_name)
    {
        if (load_exec(file_name, skip_BIOS))
            return 1;
    }
    else
    {
        is_exec_loaded = false;
    }

    //Initialize window
    is_running = true;
    setWindowTitle("DobieStation");
    resize(640, 448);
    show();
    return 0;
}

int EmuWindow::load_exec(const char* file_name, bool skip_BIOS)
{
    e.reset();

    ifstream exec_file(file_name, ios::binary | ios::in);
    if (!exec_file.is_open())
    {
        printf("Failed to load %s\n", file_name);
        return 1;
    }

    //Basic file format detection
    string file_string = file_name;
    string format = file_string.substr(file_string.length() - 4);
    transform(format.begin(), format.end(), format.begin(), ::tolower);
    printf("%s\n", format.c_str());

    if (format == ".elf")
    {
        long long ELF_size = filesize(file_name);
        uint8_t* ELF = new uint8_t[ELF_size];
        exec_file.read((char*)ELF, ELF_size);
        exec_file.close();

        printf("Loaded %s\n", file_name);
        printf("Size: %lld\n", ELF_size);
        e.load_ELF(ELF, ELF_size);
        delete[] ELF;
        ELF = nullptr;
        if (skip_BIOS)
            e.set_skip_BIOS_hack(SKIP_HACK::LOAD_ELF);
    }
    else if (format == ".iso")
    {
        exec_file.close();
        e.load_CDVD(file_name);
        if (skip_BIOS)
            e.set_skip_BIOS_hack(SKIP_HACK::LOAD_DISC);
    }
    else
    {
        printf("Unrecognized file format %s\n", format.c_str());
        return 1;
    }

    is_exec_loaded = true;
    return 0;
}

bool EmuWindow::exec_loaded()
{
    return is_exec_loaded;
}

bool EmuWindow::running()
{
    return is_running;
}

void EmuWindow::emulate()
{
    e.run();
    update();
}

void EmuWindow::create_menu()
{
    load_rom_action = new QAction(tr("&Load ROM... (Fast)"), this);
    connect(load_rom_action, &QAction::triggered, this, &EmuWindow::open_file_skip);

    load_bios_action = new QAction(tr("&Load ROM... (Boot BIOS)"), this);
    connect(load_bios_action, &QAction::triggered, this, &EmuWindow::open_file_no_skip);

    exit_action = new QAction(tr("&Exit"), this);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    file_menu = menuBar()->addMenu(tr("&File"));
    file_menu->addAction(load_rom_action);
    file_menu->addAction(load_bios_action);
    file_menu->addAction(exit_action);
}

void EmuWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    uint32_t* buffer = e.get_framebuffer();
    painter.fillRect(rect(), Qt::black);
    if (!buffer)
        return;

    //Get the resolution of the GS image
    int inner_w, inner_h;
    e.get_inner_resolution(inner_w, inner_h);

    if (!inner_w || !inner_h)
        return;

    printf("Draw image!\n");

    //Scale it to the TV screen
    QImage image((uint8_t*)buffer, inner_w, inner_h, QImage::Format_RGBA8888);

    int new_w, new_h;
    e.get_resolution(new_w, new_h);
    image = image.scaled(new_w, new_h);
    resize(new_w, new_h);

    painter.drawPixmap(0, 0, QPixmap::fromImage(image));
}

void EmuWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    is_running = false;
}

double EmuWindow::get_frame_rate()
{
    //Returns the framerate since old_frametime was set to the current time
    chrono::system_clock::time_point now = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = now - old_frametime;
    return (1 / elapsed_seconds.count());
}

void EmuWindow::update_window_title()
{
    /*
    Updates window title every second
    Framerate displayed is the average framerate over 1 second
    */
    chrono::system_clock::time_point now = chrono::system_clock::now();
    chrono::duration<double> elapsed_update_seconds = now - old_update_time;

    double framerate = get_frame_rate();
    framerate_avg = (framerate_avg + framerate) / 2;
    if (elapsed_update_seconds.count() >= 1.0)
    {
        int rf = (int) round(framerate_avg); // rounded framerate
        string new_title = "DobieStation - FPS: " + to_string(rf);
        setWindowTitle(QString::fromStdString(new_title));
        old_update_time = chrono::system_clock::now();
        framerate_avg = framerate;
    }
}

void EmuWindow::limit_frame_rate()
{
    while (get_frame_rate() > 60.0);
}

void EmuWindow::reset_frame_time()
{
    old_frametime = chrono::system_clock::now();
}

#ifndef QT_NO_CONTEXTMENU
void EmuWindow::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(load_rom_action);
    menu.addAction(load_bios_action);
    menu.addAction(exit_action);
    menu.exec(event->globalPos());
}
#endif // QT_NO_CONTEXTMENU

void EmuWindow::open_file_no_skip()
{
    QString file_name = QFileDialog::getOpenFileName(this, tr("Open Rom"), "", tr("ROM Files (*.elf *.iso)"));
    load_exec(file_name.toStdString().c_str(), false);
}

void EmuWindow::open_file_skip()
{
    QString file_name = QFileDialog::getOpenFileName(this, tr("Open Rom"), "", tr("ROM Files (*.elf *.iso)"));
    load_exec(file_name.toStdString().c_str(), true);
}