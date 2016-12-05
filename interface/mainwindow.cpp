#include <QMessageBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QActionGroup>
#include <QCloseEvent>
#include <cstdio>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "renderers/renwin3d.h"
#include "renderers/renwin2d.h"
#include "mesh/mesh.h"
#include "settingswindow.h"
#include "settings/settings.h"
#include "scalewindow.h"
#include "interface/materialmanager.h"

extern QString g_GetSupported3DFormats();

CMainWindow::CMainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_model(nullptr)
{
    ui->setupUi(this);
    m_rw3 = new CRenWin3D(ui->frameLeft);
    m_rw2 = new CRenWin2D(ui->frameRight);
    {
        QHBoxLayout* l = new QHBoxLayout(ui->frameLeft);
        l->addWidget(m_rw3);
        ui->frameLeft->setLayout(l);
    }
    {
        QHBoxLayout* l = new QHBoxLayout(ui->frameRight);
        l->addWidget(m_rw2);
        ui->frameRight->setLayout(l);
    }

    QActionGroup *ag = new QActionGroup(this);
    ag->addAction(ui->actionModeMove);
    ag->addAction(ui->actionModeRotate);
    ag->addAction(ui->actionModeSnap);
    ag->addAction(ui->actionModeFlaps);
    ag->addAction(ui->actionModeAddSheet);
    ag->addAction(ui->actionModeMoveSheet);
    ag->addAction(ui->actionModeRemSheet);
    ui->actionModeMove->setChecked(true);

    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionOpen_obj, SIGNAL(triggered()), this, SLOT(LoadModel()));
    connect(ui->actionLoad_Texture, SIGNAL(triggered()), this, SLOT(OpenMaterialManager()));
    connect(this, SIGNAL(UpdateTexture(QImage*, unsigned)), m_rw3, SLOT(LoadTexture(QImage*, unsigned)));
    connect(this, SIGNAL(UpdateTexture(QImage*, unsigned)), m_rw2, SLOT(LoadTexture(QImage*, unsigned)));
}

CMainWindow::~CMainWindow()
{
    delete ui;
    if(m_model)
        delete m_model;
}

void CMainWindow::closeEvent(QCloseEvent *event)
{
    AskToSaveChanges();
    event->accept();
}

void CMainWindow::OpenMaterialManager()
{
    if(!m_model)
        return;
    CMaterialManager matMngr(this);
    matMngr.SetMaterials(m_model->GetMaterials(), m_textures);
    matMngr.exec();

    auto newTextures = matMngr.GetTextures();
    for(auto it=newTextures.begin(); it!=newTextures.end(); it++)
    {
        if(m_textures[it->first] != it->second)
        {
            m_textures[it->first] = it->second;
            if(!it->second.empty())
            {
                m_textureImages[it->first].reset(new QImage(it->second.c_str()));
                (*(m_textureImages[it->first])) = m_textureImages[it->first]->convertToFormat(QImage::Format_RGB32);
            } else {
                m_textureImages[it->first].reset(nullptr);
            }
            emit UpdateTexture(m_textureImages[it->first].get(), it->first);
        }
    }
}

void CMainWindow::ClearTextures()
{
    m_textures.clear();
    m_textureImages.clear();
    m_rw2->ClearTextures();
    m_rw3->ClearTextures();

    if(m_model)
    {
        auto& materials = m_model->GetMaterials();
        for(auto it=materials.begin(); it!=materials.end(); it++)
        {
            m_textures[it->first] = "";
            m_textureImages[it->first] = nullptr;
            m_rw2->ReserveTextureID(it->first);
            m_rw3->ReserveTextureID(it->first);
        }
    }
}

void CMainWindow::LoadModel()
{
    std::string modelPath = QFileDialog::getOpenFileName(this,
                                                         "Open Model",
                                                         "",
                                                         g_GetSupported3DFormats()).toStdString();
    if(modelPath.empty())
        return;

    CMesh *newModel = new CMesh();
    std::string errorString = newModel->LoadMesh(modelPath);
    if(!errorString.empty())
    {
        delete newModel;
        QMessageBox::information(this, "Error", errorString.c_str());
    } else {
        AskToSaveChanges();
        m_openedModel = "";
        m_rw2->SetModel(newModel);
        m_rw3->SetModel(newModel);
        if(m_model)
            delete m_model;
        m_model = newModel;
        ClearTextures();

        UpdateView();
    }
}

void CMainWindow::AskToSaveChanges()
{
    if(m_model)
    {
        if(m_model->IsModified() || m_openedModel == "")
        {
            auto result = QMessageBox::question(this, "Save changes", "Do you want to save changes to current model?",
                                                QMessageBox::Yes | QMessageBox::No);
            if(result == QMessageBox::Yes)
            {
                on_actionSave_triggered();
            }
        }
    }
}

void CMainWindow::on_actionModeRotate_triggered()
{
    m_rw2->SetMode(CRenWin2D::EM_ROTATE);
}

void CMainWindow::on_actionModeSnap_triggered()
{
    m_rw2->SetMode(CRenWin2D::EM_SNAP);
}

void CMainWindow::on_actionModeMove_triggered()
{
    m_rw2->SetMode(CRenWin2D::EM_MOVE);
}

void CMainWindow::on_actionModeFlaps_triggered()
{
    m_rw2->SetMode(CRenWin2D::EM_CHANGE_FLAPS);
}

void CMainWindow::on_actionModeAddSheet_triggered()
{
    m_rw2->SetMode(CRenWin2D::EM_ADD_SHEET);
}

void CMainWindow::on_actionModeMoveSheet_triggered()
{
    m_rw2->SetMode(CRenWin2D::EM_MOVE_SHEET);
}

void CMainWindow::on_actionModeRemSheet_triggered()
{
    m_rw2->SetMode(CRenWin2D::EM_REM_SHEET);
}

void CMainWindow::on_actionExport_Sheets_triggered()
{
    if(m_openedModel.empty())
    {
        m_rw2->ExportSheets("untitled");
    } else {
        m_rw2->ExportSheets(QFileInfo(m_openedModel.c_str()).baseName());
    }
}

void CMainWindow::on_actionSettings_triggered()
{
    CSettingsWindow sw(this);
    sw.LoadSettings();
    sw.exec();
    m_rw2->UpdateSheetsSize();
    UpdateView();
}

void CMainWindow::on_actionZoom_fit_triggered()
{
    m_rw2->ZoomFit();
    m_rw3->ZoomFit();
}

void CMainWindow::on_actionZoom_2D_triggered()
{
    m_rw2->ZoomFit();
}

void CMainWindow::on_actionZoom_3D_triggered()
{
    m_rw3->ZoomFit();
}

void CMainWindow::on_actionUndo_triggered()
{
    if(m_model)
    {
        m_model->Undo();
        UpdateView();
    }
}

void CMainWindow::on_actionRedo_triggered()
{
    if(m_model)
    {
        m_model->Redo();
        UpdateView();
    }
}

void CMainWindow::Serialize(const char* filename)
{
    FILE* f = std::fopen(filename, "wb");
    if(!f)
        return;

    char ivo[4] = "IVO";
    int version = IVO_VERSION;

    unsigned char renFlags = CSettings::GetInstance().GetRenderFlags();
    unsigned      paperWid = CSettings::GetInstance().GetPaperWidth();
    unsigned      paperHei = CSettings::GetInstance().GetPaperHeight();
    float         resScale = CSettings::GetInstance().GetResolutionScale();
    int           imageFmt = CSettings::GetInstance().GetImageFormat();
    unsigned char imageQlt = CSettings::GetInstance().GetImageQuality();
    float         lineWidt = CSettings::GetInstance().GetLineWidth();
    unsigned      stipplLp = CSettings::GetInstance().GetStippleLoop();


    //header + version
    std::fwrite(ivo, sizeof(char), 3, f);
    std::fwrite(&version, sizeof(int), 1, f);
    //settings
    std::fwrite(&renFlags, sizeof(renFlags), 1, f);
    std::fwrite(&paperWid, sizeof(paperWid), 1, f);
    std::fwrite(&paperHei, sizeof(paperHei), 1, f);
    std::fwrite(&resScale, sizeof(resScale), 1, f);
    std::fwrite(&imageFmt, sizeof(imageFmt), 1, f);
    std::fwrite(&imageQlt, sizeof(imageQlt), 1, f);
    std::fwrite(&lineWidt, sizeof(lineWidt), 1, f);
    std::fwrite(&stipplLp, sizeof(stipplLp), 1, f);
    //mesh data
    m_model->Serialize(f);

    m_rw2->SerializeSheets(f);

    auto materials = m_model->GetMaterials();
    unsigned char numTextures = (unsigned char)materials.size();
    std::fwrite(&numTextures, sizeof(numTextures), 1, f);
    for(auto it=materials.begin(); it!=materials.end(); it++)
    {
        unsigned key = it->first;
        std::string matName = it->second;
        std::string texPath = m_textures[key];
        const QImage* image = m_textureImages[key].get();

        int nameLen = matName.length();
        int pathLen = texPath.length();

        std::fwrite(&key, sizeof(key), 1, f);
        std::fwrite(&nameLen, sizeof(nameLen), 1, f);
        std::fwrite(matName.data(), sizeof(char), nameLen, f);
        std::fwrite(&pathLen, sizeof(pathLen), 1, f);
        std::fwrite(texPath.data(), sizeof(char), pathLen, f);

        int hasTexture = (image != nullptr ? 1 : 0);
        std::fwrite(&hasTexture, sizeof(hasTexture), 1, f);

        if(image)
        {
            int texWidth = image->width();
            int texHeight = image->height();
            int texByteCount = image->byteCount();
            int texFormat = (int)image->format();

            std::fwrite(&texWidth, sizeof(texWidth), 1, f);
            std::fwrite(&texHeight, sizeof(texHeight), 1, f);
            std::fwrite(&texByteCount, sizeof(texByteCount), 1, f);
            std::fwrite(&texFormat, sizeof(texFormat), 1, f);
            std::fwrite(image->constBits(), sizeof(unsigned char), texByteCount, f);
        }
    }

    m_openedModel = filename;

    std::fclose(f);
}

void CMainWindow::Deserialize(const char* filename)
{
    FILE* f = std::fopen(filename, "rb");
    if(!f)
        return;

    AskToSaveChanges();

    char ivo[4];
    int version = -1;

    std::fread(ivo, sizeof(char), 3, f);
    ivo[3] = '\0';
    if(strcmp(ivo, "IVO") != 0)
    {
        QMessageBox::information(this, "Error", "Selected file is not a valid IVO model!");
        std::fclose(f);
        return;
    }
    std::fread(&version, sizeof(version), 1, f);

    switch(version)
    {
        case 1 :
        {
            unsigned char renFlags;
            unsigned      paperWid;
            unsigned      paperHei;
            float         resScale;
            int           imageFmt;
            unsigned char imageQlt;
            float         lineWidt;
            unsigned      stipplLp;

            std::fread(&renFlags, sizeof(renFlags), 1, f);
            std::fread(&paperWid, sizeof(paperWid), 1, f);
            std::fread(&paperHei, sizeof(paperHei), 1, f);
            std::fread(&resScale, sizeof(resScale), 1, f);
            std::fread(&imageFmt, sizeof(imageFmt), 1, f);
            std::fread(&imageQlt, sizeof(imageQlt), 1, f);
            std::fread(&lineWidt, sizeof(lineWidt), 1, f);
            std::fread(&stipplLp, sizeof(stipplLp), 1, f);

            CMesh *newModel = new CMesh();
            newModel->Deserialize(f);

            CSettings& sett = CSettings::GetInstance();

            sett.SetRenderFlags( renFlags );
            sett.SetPaperWidth( paperWid );
            sett.SetPaperHeight( paperHei );
            sett.SetResolutionScale( resScale );
            sett.SetImageFormat( (CSettings::ImageFormat)imageFmt );
            sett.SetImageQuality( imageQlt );
            sett.SetLineWidth( lineWidt );
            sett.SetStippleLoop( stipplLp );

            m_openedModel = filename;

            m_rw2->SetModel(newModel);
            m_rw3->SetModel(newModel);
            if(m_model)
                delete m_model;
            m_model = newModel;
            ClearTextures();

            m_rw2->DeserializeSheets(f);
            m_rw2->UpdateSheetsSize();

            std::unordered_map<unsigned, std::string> materials;

            unsigned char numTextures = 0;
            std::fread(&numTextures, sizeof(numTextures), 1, f);
            for(int i=numTextures-1; i>=0; i--)
            {
                unsigned key;
                int nameLen;
                int pathLen;
                std::fread(&key, sizeof(key), 1, f);

                std::fread(&nameLen, sizeof(nameLen), 1, f);
                char* name = new char[nameLen+1];
                name[nameLen] = '\0';
                std::fread(name, sizeof(char), nameLen, f);

                std::fread(&pathLen, sizeof(pathLen), 1, f);
                char* path = new char[pathLen+1];
                path[pathLen] = '\0';
                std::fread(path, sizeof(char), pathLen, f);

                int hasTexture = 0;
                std::fread(&hasTexture, sizeof(hasTexture), 1, f);

                if(hasTexture == 1)
                {
                    int texWidth = 0;
                    int texHeight = 0;
                    int texByteCount = 0;
                    int texFormat = 0;

                    std::fread(&texWidth, sizeof(texWidth), 1, f);
                    std::fread(&texHeight, sizeof(texHeight), 1, f);
                    std::fread(&texByteCount, sizeof(texByteCount), 1, f);
                    std::fread(&texFormat, sizeof(texFormat), 1, f);

                    unsigned char* imgBits = new unsigned char[texByteCount];
                    std::fread(imgBits, sizeof(unsigned char), texByteCount, f);

                    m_textureImages[key].reset(new QImage(imgBits, texWidth, texHeight, (QImage::Format)texFormat));
                    (*m_textureImages[key]) = m_textureImages[key]->copy();

                    delete[] imgBits;
                }

                materials[key] = name;
                m_textures[key] = path;

                m_rw2->ReserveTextureID(key);
                m_rw3->ReserveTextureID(key);

                if(hasTexture == 1)
                    emit UpdateTexture(m_textureImages[key].get(), key);
            }

            m_model->SetMaterials(materials);

            UpdateView();

            break;
        }
        default :
        {
            QMessageBox::information(this, "Error", "Ivo format version " + QString::number(version) + " is not supported by this version of program!");
        }
    }

    std::fclose(f);
}

void CMainWindow::on_actionSave_As_triggered()
{
    if(!m_model)
        return;

    std::string filePath = QFileDialog::getSaveFileName(this,
                                                         "Save As",
                                                         "",
                                                         "Ivo model (*.ivo)").toStdString();
    if(filePath.empty())
        return;

    if(filePath.length() < 4 || filePath.substr(filePath.length()-4) != ".ivo")
    {
        filePath += ".ivo";
    }

    Serialize(filePath.c_str());
}

void CMainWindow::on_actionSave_triggered()
{
    if(m_openedModel.empty())
    {
        on_actionSave_As_triggered();
    } else {
        Serialize(m_openedModel.c_str());
    }
}

void CMainWindow::on_actionLoad_Model_triggered()
{
    std::string ivoModelPath = QFileDialog::getOpenFileName(this,
                                                         "Open Ivo Model",
                                                         "",
                                                         "Ivo model (*.ivo)").toStdString();
    if(ivoModelPath.empty())
        return;

    Deserialize(ivoModelPath.c_str());
}

void CMainWindow::on_actionScale_triggered()
{
    if(m_model)
    {
        float outScale = 1.0f;

        CScaleWindow scaleWnd(this);
        scaleWnd.SetOutputScalePtr(&outScale);
        scaleWnd.SetInitialScale(m_model->GetSizeMillimeters());
        scaleWnd.exec();

        if(outScale != 1.0f)
        {
            m_model->Scale(outScale);
        }
        UpdateView();
    }
}

void CMainWindow::UpdateView()
{
    m_rw2->update();
    m_rw3->update();
    update();
}

void CMainWindow::on_actionAutoPack_triggered()
{
    if(m_model)
    {
        m_model->PackGroups();
        UpdateView();
    }
}

void CMainWindow::on_actionShow_Grid_triggered(bool checked)
{
    CSettings::RenderGrid = checked;
    UpdateView();
}
