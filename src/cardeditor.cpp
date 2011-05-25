#include "cardeditor.h"
#include "mainwindow.h"
#include "engine.h"
#include "settings.h"

#include <QHBoxLayout>
#include <QFormLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QCommandLinkButton>
#include <QFontDatabase>
#include <QGraphicsDropShadowEffect>
#include <QLabel>

CardScene::CardScene()
    :QGraphicsScene(QRectF(0, 0, 366, 514))
{
    photo = new QGraphicsPixmapItem;
    frame = new QGraphicsPixmapItem;

    name = new QGraphicsTextItem;
    name->setFlag(QGraphicsItem::ItemIsMovable);
    name->setPos(28, 206);
    name->setTextWidth(36);
    name->setDefaultTextColor(Qt::white);

    QGraphicsDropShadowEffect *effect = new QGraphicsDropShadowEffect;
    effect->setXOffset(-1);
    effect->setYOffset(-1);
    effect->setBlurRadius(10);
    effect->setColor(Qt::black);


    name->setGraphicsEffect(effect);

    title = new QGraphicsTextItem;


    photo->setFlag(QGraphicsItem::ItemIsMovable);

    addItem(photo);
    addItem(frame);
    addItem(name);
    addItem(title);

    int i;
    for(i=0; i<8; i++){
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem;
        magatamas << item;
        item->hide();
        addItem(item);

        item->setPos(94 + i*(115-94), 18);
    }
}

void CardScene::setFrame(const QString &kingdom, bool is_lord){
    QString path;
    if(is_lord)
        path = QString("diy/%1-lord.png").arg(kingdom);
    else
        path = QString("diy/%1.png").arg(kingdom);

    frame->setPixmap(QPixmap(path));

    foreach(QGraphicsPixmapItem *item, magatamas){
        item->setPixmap(QPixmap(QString("diy/%1-magatama.png").arg(kingdom)));
    }
}

void CardScene::setGeneralPhoto(const QString &filename){
    photo->setPixmap(QPixmap(filename));
}

void CardScene::setName(const QString &name){
    this->name->setPlainText(name);
}

void CardScene::setNameFont(const QString &family){
    QFont font(family);
    font.setPointSize(36);
    name->setFont(font);
}

void CardScene::setTitleFont(const QString &family){

}

void CardScene::setTitle(const QString &title){

}

void CardScene::setMaxHp(int max_hp){    
    int n = magatamas.length();
    max_hp = qBound(0, max_hp, n-1);

    int i;
    for(i=0; i<n; i++)
        magatamas.at(i)->setVisible(i < max_hp);
}

void CardScene::setRatio(int ratio){
    photo->setScale(ratio / 100.0);
}

static QLayout *HLay(QWidget *left, QWidget *right){
    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(left);
    layout->addWidget(right);

    return layout;
}

CardEditor::CardEditor(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Card editor"));

    QHBoxLayout *layout = new QHBoxLayout;
    QGraphicsView *view = new QGraphicsView;
    card_scene = new CardScene;
    view->setScene(card_scene);
    view->setMinimumSize(380, 530);

    layout->addWidget(createLeft());
    layout->addWidget(view);

    setLayout(layout);

    card_scene->setFrame("wei", false);
}

QGroupBox *CardEditor::createLeft(){
    QGroupBox *box = new QGroupBox;
    box->setTitle(tr("Properties"));

    QFontDatabase db;

    name_edit = new QLineEdit;
    QComboBox *name_font_combobox = createFontCombobox(db);

    title_edit = new QLineEdit;
    QComboBox *title_font_combobox = createFontCombobox(db);

    kingdom_combobox = new QComboBox;
    lord_checkbox = new QCheckBox(tr("Lord"));
    foreach(QString kingdom, Sanguosha->getKingdoms()){
        QIcon icon(QString("image/kingdom/icon/%1.png").arg(kingdom));
        kingdom_combobox->addItem(icon, Sanguosha->translate(kingdom), kingdom);
    }

    hp_spinbox = new QSpinBox;
    hp_spinbox->setRange(0, 8);
    hp_spinbox->setValue(3);

    QPushButton *browse_button = new QPushButton(tr("Photo file ..."));
    ratio_spinbox = new QSpinBox;
    ratio_spinbox->setRange(1, 1600);
    ratio_spinbox->setValue(100);
    ratio_spinbox->setSuffix(" %");

    QFormLayout *layout = new QFormLayout;
    layout->addRow(tr("Name"), HLay(name_edit, name_font_combobox));
    layout->addRow(tr("Title"), HLay(title_edit, title_font_combobox));

    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->addWidget(new QLabel(tr("Kingdom")));
    hlayout->addWidget(kingdom_combobox);
    hlayout->addWidget(lord_checkbox);
    hlayout->addWidget(new QLabel(tr("Max HP")));
    hlayout->addWidget(hp_spinbox);
    layout->addRow(hlayout);

    layout->addRow(tr("General"), HLay(browse_button, ratio_spinbox));

    skill_tabs = new QTabWidget;

    int i;
    for(i=1; i<=4; i++){
        skill_tabs->addTab(createSkillTab(), tr("Skill %1").arg(i));
    }

    layout->addRow(skill_tabs);

    QCommandLinkButton *save_button = new QCommandLinkButton(tr("Save"));
    save_button->setDescription(tr("Save the image"));
    layout->addRow(save_button);

    connect(kingdom_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(setCardFrame()));
    connect(lord_checkbox, SIGNAL(toggled(bool)), this, SLOT(setCardFrame()));
    connect(name_edit, SIGNAL(textChanged(QString)), card_scene, SLOT(setName(QString)));
    connect(title_edit, SIGNAL(textChanged(QString)), card_scene, SLOT(setTitle(QString)));
    connect(browse_button, SIGNAL(clicked()), this, SLOT(browseGeneralPhoto()));
    connect(hp_spinbox, SIGNAL(valueChanged(int)), card_scene, SLOT(setMaxHp(int)));
    connect(ratio_spinbox, SIGNAL(valueChanged(int)), card_scene, SLOT(setRatio(int)));
    connect(save_button, SIGNAL(clicked()), this, SLOT(saveImage()));
    connect(name_font_combobox, SIGNAL(currentIndexChanged(QString)), card_scene, SLOT(setNameFont(QString)));
    connect(title_font_combobox, SIGNAL(currentIndexChanged(QString)), card_scene, SLOT(setTitleFont(QString)));

    box->setLayout(layout);
    return box;
}

QComboBox *CardEditor::createFontCombobox(const QFontDatabase &db){
    QComboBox *combobox = new QComboBox;
    QStringList families = db.families(QFontDatabase::SimplifiedChinese);
    families << db.families(QFontDatabase::TraditionalChinese);
    families.sort();
    combobox->addItems(families);

    return combobox;
}

void CardEditor::setCardFrame(){
    QString kingdom = kingdom_combobox->itemData(kingdom_combobox->currentIndex()).toString();
    if(kingdom == "god")
        card_scene->setFrame("god", false);
    else
        card_scene->setFrame(kingdom, lord_checkbox->isChecked());
}

QWidget *CardEditor::createSkillTab(){
    QWidget *tab = new QWidget;
    QFormLayout *layout = new QFormLayout;

    QLineEdit *skill_name_edit = new QLineEdit;
    QTextEdit *description_edit = new QTextEdit;

    layout->addRow(tr("Name"), skill_name_edit);
    layout->addRow(tr("Description"), description_edit);

    tab->setLayout(layout);

    return tab;
}

void CardEditor::browseGeneralPhoto(){
    QString filename = QFileDialog::getOpenFileName(this,
                                                    tr("Select a photo file ..."),
                                                    QString(),
                                                    tr("Images (*.png *.bmp *.jpg)")
                                                    );

    if(!filename.isEmpty())
        card_scene->setGeneralPhoto(filename);
}

void CardEditor::saveImage(){
    QString filename = QFileDialog::getSaveFileName(this,
                                                    tr("Select a photo file ..."),
                                                    QString(),
                                                    tr("Images (*.png *.bmp *.jpg)")
                                                    );

    if(!filename.isEmpty()){
        QImage image(card_scene->sceneRect().size().toSize(), QImage::Format_ARGB32);
        QPainter painter(&image);
        card_scene->render(&painter);

        image.save(filename);
    }
}

void MainWindow::on_actionCard_editor_triggered()
{
    CardEditor *editor = new CardEditor(this);
    editor->exec();
}