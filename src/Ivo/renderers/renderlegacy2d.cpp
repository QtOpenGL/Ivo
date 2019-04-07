/*
    Ivo - a free software for unfolding 3D models and papercrafting
    Copyright (C) 2015-2018 Oleksii Sierov (seriousalexej@gmail.com)
	
    Ivo is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Ivo is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with Ivo.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <QOpenGLFramebufferObject>
#include <stdexcept>
#include <typeinfo>
#include <typeindex>
#include "renderlegacy2d.h"
#include "mesh/mesh.h"
#include "settings/settings.h"
#include "interface/renwin2d.h"
#include "interface/editinfo2d.h"
#include "interface/modes2D/snap.h"
#include "interface/modes2D/flaps.h"
#include "interface/modes2D/rotate.h"
#include "interface/modes2D/select.h"
#include "geometric/aabbox.h"

CRenderer2DLegacy::CRenderer2DLegacy(QOpenGLFunctions_2_0& gl) :
    m_gl(gl)
{
}

CRenderer2DLegacy::~CRenderer2DLegacy()
{
}

void CRenderer2DLegacy::Init()
{
    m_gl.glEnable(GL_DEPTH_TEST);
    m_gl.glDepthFunc(GL_LEQUAL);
    m_gl.glEnable(GL_TEXTURE_2D);
    m_gl.glEnable(GL_MULTISAMPLE);
    m_gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_gl.glEnable(GL_BLEND);
    m_gl.glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
    CreateFoldTextures();
}

void CRenderer2DLegacy::ResizeView(int w, int h)
{
    m_width = w;
    m_height = h;
    m_gl.glViewport(0, 0, w, h);
}

void CRenderer2DLegacy::RecalcProjection()
{
    m_gl.glMatrixMode(GL_PROJECTION);
    m_gl.glLoadIdentity();
    const float hheight = m_cameraPosition[2] * float(m_height)/float(m_width);
    m_gl.glOrtho(-m_cameraPosition[2], m_cameraPosition[2], -hheight, hheight, 0.1f, 2000.0f);
}

void CRenderer2DLegacy::PreDraw() const
{
    m_gl.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_gl.glMatrixMode(GL_MODELVIEW);
    m_gl.glLoadIdentity();

    m_gl.glTranslatef(m_cameraPosition[0], m_cameraPosition[1], -1.0f);

    const float hwidth = m_cameraPosition[2];
    const float hheight = hwidth * float(m_height)/float(m_width);
    if(m_cameraPosition.y - hheight < 0.0f && m_cameraPosition.y + hheight > 0.0f)
    {
        m_gl.glColor3f(0.0f, 0.0f, 0.0f);
        m_gl.glBegin(GL_LINES);
        m_gl.glVertex3f(-m_cameraPosition.x - hwidth, 0.0f, -10.0f);
        m_gl.glVertex3f(-m_cameraPosition.x + hwidth, 0.0f, -10.0f);
        m_gl.glEnd();
        m_gl.glClear(GL_DEPTH_BUFFER_BIT);
    }
    if(m_cameraPosition.x - hwidth < 0.0f && m_cameraPosition.x + hwidth > 0.0f)
    {
        m_gl.glColor3f(0.0f, 0.0f, 0.0f);
        m_gl.glBegin(GL_LINES);
        m_gl.glVertex3f(0.0f, -m_cameraPosition.y - hheight, -10.0f);
        m_gl.glVertex3f(0.0f, -m_cameraPosition.y + hheight, -10.0f);
        m_gl.glEnd();
        m_gl.glClear(GL_DEPTH_BUFFER_BIT);
    }
    m_gl.glColor3f(1.0f, 1.0f, 1.0f);
}

void CRenderer2DLegacy::DrawSelection(const SEditInfo& sinfo) const
{
    if(!m_model)
        return;

    m_gl.glClear(GL_DEPTH_BUFFER_BIT);

    static const std::type_index tiSnap(typeid(CModeSnap));
    static const std::type_index tiFlaps(typeid(CModeFlaps));
    static const std::type_index tiRotate(typeid(CModeRotate));
    static const std::type_index tiSelect(typeid(CModeSelect));

    if(sinfo.editMode == tiSnap  ||
       sinfo.editMode == tiFlaps ||
       sinfo.editMode == tiRotate)
    {
        CMesh::STriangle2D* trUnderCursor = nullptr;
        int edgeUnderCursor = 0;
        m_model->GetStuffUnderCursor(sinfo.mousePosition, trUnderCursor, edgeUnderCursor);

        if(sinfo.editMode == tiRotate && sinfo.currTri)
        {
            trUnderCursor = sinfo.currTri;
            edgeUnderCursor = sinfo.currEdge;
        }

        //highlight edge under cursor (if it has neighbour)
        if(trUnderCursor && (trUnderCursor->GetEdge(edgeUnderCursor)->HasTwoTriangles() || sinfo.editMode == tiRotate) &&
           (sinfo.editMode != tiFlaps || !trUnderCursor->GetEdge(edgeUnderCursor)->IsSnapped()))
        {
            const glm::vec2 &v1 = (*trUnderCursor)[0];
            const glm::vec2 &v2 = (*trUnderCursor)[1];
            const glm::vec2 &v3 = (*trUnderCursor)[2];

            glm::vec2 e1Middle;

            if(sinfo.editMode == tiFlaps)
            {
                m_gl.glColor3f(0.0f, 0.0f, 1.0f);
            } else if(sinfo.editMode == tiSnap) {
                if(trUnderCursor->GetEdge(edgeUnderCursor)->IsSnapped())
                    m_gl.glColor3f(1.0f, 0.0f, 0.0f);
                else
                    m_gl.glColor3f(0.0f, 1.0f, 0.0f);
            } else {
                m_gl.glColor3f(0.0f, 1.0f, 1.0f);
            }
            m_gl.glLineWidth(3.0f);
            m_gl.glBegin(GL_LINES);
            switch(edgeUnderCursor)
            {
                case 0:
                    m_gl.glVertex2f(v1[0], v1[1]);
                    m_gl.glVertex2f(v2[0], v2[1]);
                    e1Middle = 0.5f*(v1+v2);
                    break;
                case 1:
                    m_gl.glVertex2f(v3[0], v3[1]);
                    m_gl.glVertex2f(v2[0], v2[1]);
                    e1Middle = 0.5f*(v3+v2);
                    break;
                case 2:
                    m_gl.glVertex2f(v1[0], v1[1]);
                    m_gl.glVertex2f(v3[0], v3[1]);
                    e1Middle = 0.5f*(v1+v3);
                    break;
                default : break;
            }

            if(sinfo.editMode != tiRotate)
            {
                const CMesh::STriangle2D* tr2 = trUnderCursor->GetEdge(edgeUnderCursor)->GetOtherTriangle(trUnderCursor);
                int e2 = trUnderCursor->GetEdge(edgeUnderCursor)->GetOtherTriIndex(trUnderCursor);
                const glm::vec2 &v12 = (*tr2)[0];
                const glm::vec2 &v22 = (*tr2)[1];
                const glm::vec2 &v32 = (*tr2)[2];

                switch(e2)
                {
                    case 0:
                        m_gl.glVertex2f(v12[0], v12[1]);
                        m_gl.glVertex2f(v22[0], v22[1]);
                        m_gl.glVertex2f(e1Middle[0], e1Middle[1]);
                        e1Middle = 0.5f*(v12+v22);
                        m_gl.glVertex2f(e1Middle[0], e1Middle[1]);
                        break;
                    case 1:
                        m_gl.glVertex2f(v32[0], v32[1]);
                        m_gl.glVertex2f(v22[0], v22[1]);
                        m_gl.glVertex2f(e1Middle[0], e1Middle[1]);
                        e1Middle = 0.5f*(v32+v22);
                        m_gl.glVertex2f(e1Middle[0], e1Middle[1]);
                        break;
                    case 2:
                        m_gl.glVertex2f(v12[0], v12[1]);
                        m_gl.glVertex2f(v32[0], v32[1]);
                        m_gl.glVertex2f(e1Middle[0], e1Middle[1]);
                        e1Middle = 0.5f*(v12+v32);
                        m_gl.glVertex2f(e1Middle[0], e1Middle[1]);
                        break;
                    default : break;
                }
            }
            m_gl.glEnd();
            m_gl.glLineWidth(1.0f);
            m_gl.glColor3f(1.0f, 1.0f, 1.0f);
        }
    }

    //draw selection rectangles
    if(!sinfo.selection.empty())
    {
        m_gl.glClear(GL_DEPTH_BUFFER_BIT);
        m_gl.glColor3f(1.0f, 0.0f, 0.0f);
        for(const auto* tGroup : sinfo.selection)
        {
            const SAABBox2D bbox = tGroup->GetAABBox();

            m_gl.glBegin(GL_LINE_LOOP);
            m_gl.glVertex2f(bbox.GetLeft(), bbox.GetTop());
            m_gl.glVertex2f(bbox.GetRight(), bbox.GetTop());
            m_gl.glVertex2f(bbox.GetRight(), bbox.GetBottom());
            m_gl.glVertex2f(bbox.GetLeft(), bbox.GetBottom());
            m_gl.glEnd();
        }
        m_gl.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    //draw selection area
    if(sinfo.editMode == tiSelect && sinfo.modeIsActive)
    {
        const glm::vec2& pos_1 = sinfo.mousePressPoint;
        const glm::vec2& pos_2 = sinfo.mousePosition;

        m_gl.glClear(GL_DEPTH_BUFFER_BIT);
        m_gl.glEnable(GL_BLEND);
        m_gl.glColor4f(0.63926f, 0.82353f, 1.0f, 0.62353f);
        m_gl.glBegin(GL_QUADS);
        m_gl.glVertex2f(pos_1.x, pos_1.y);
        m_gl.glVertex2f(pos_2.x, pos_1.y);
        m_gl.glVertex2f(pos_2.x, pos_2.y);
        m_gl.glVertex2f(pos_1.x, pos_2.y);
        m_gl.glEnd();
        m_gl.glColor3f(1.0f, 1.0f, 1.0f);
    }
}

void CRenderer2DLegacy::DrawPaperSheets(unsigned numHorizontal, unsigned numVertical) const
{
    if(numHorizontal == 0 || numVertical == 0)
        return;

    const CSettings& sett = CSettings::GetInstance();
    const float papHeight = sett.GetPaperHeight() * 0.1f;
    const float papWidth = sett.GetPaperWidth() * 0.1f;
    const float papHMargins = sett.GetMarginsHorizontal() * 0.1f;
    const float papVMargins = sett.GetMarginsVertical() * 0.1f;
    const glm::vec2 size = glm::vec2(papWidth * numHorizontal, papHeight * numVertical);
    const glm::vec2 position(0.0f, -size.y);

    m_gl.glColor3f(0.0f, 0.0f, 0.0f);
    m_gl.glBegin(GL_LINE_LOOP);
    m_gl.glVertex3f(position.x, position.y, -3.0f);
    m_gl.glVertex3f(position.x+size.x, position.y, -3.0f);
    m_gl.glVertex3f(position.x+size.x, position.y+size.y, -3.0f);
    m_gl.glVertex3f(position.x, position.y+size.y, -3.0f);
    m_gl.glEnd();
    m_gl.glColor3f(0.7f, 0.7f, 0.7f);
    m_gl.glBegin(GL_LINES);
    for(unsigned i=1; i<numHorizontal; i++)
    {
        m_gl.glVertex3f(position.x + papWidth * i, position.y, -2.0f);
        m_gl.glVertex3f(position.x + papWidth * i, 0.0f, -2.0f);
    }
    for(unsigned i=1; i<numVertical; i++)
    {
        m_gl.glVertex3f(position.x, -papHeight * i, -2.0f);
        m_gl.glVertex3f(position.x + size.x, -papHeight * i, -2.0f);
    }
    m_gl.glEnd();

    m_gl.glColor3f(0.9f, 0.9f, 0.9f);
    for(unsigned x=0u; x<numHorizontal; x++)
    for(unsigned y=0u; y<numVertical; y++)
    {
        m_gl.glBegin(GL_LINE_LOOP);
        m_gl.glVertex3f(position.x + papWidth*x + papHMargins, position.y + papHeight*y + papVMargins, -2.0f);
        m_gl.glVertex3f(position.x + papWidth*(x+1) - papHMargins, position.y + papHeight*y + papVMargins, -2.0f);
        m_gl.glVertex3f(position.x + papWidth*(x+1) - papHMargins, position.y + papHeight*(y+1) - papVMargins, -2.0f);
        m_gl.glVertex3f(position.x + papWidth*x + papHMargins, position.y + papHeight*(y+1) - papVMargins, -2.0f);
        m_gl.glEnd();
    }

    m_gl.glColor3f(0.2f, 0.2f, 0.2f);
    m_gl.glBegin(GL_QUADS);
    m_gl.glVertex3f(position.x+0.5f, position.y-0.5f, -10.0f);
    m_gl.glVertex3f(position.x+0.5f+size.x, position.y-0.5f, -10.0f);
    m_gl.glVertex3f(position.x+0.5f+size.x, position.y+size.y-0.5f, -10.0f);
    m_gl.glVertex3f(position.x+0.5f, position.y+size.y-0.5f, -10.0f);
    m_gl.glColor3f(1.0f, 1.0f, 1.0f);
    m_gl.glVertex3f(position.x, position.y, -5.0f);
    m_gl.glVertex3f(position.x+size.x, position.y, -5.0f);
    m_gl.glVertex3f(position.x+size.x, position.y+size.y, -5.0f);
    m_gl.glVertex3f(position.x, position.y+size.y, -5.0f);
    m_gl.glEnd();
}

void CRenderer2DLegacy::DrawScene() const
{
    if(!m_model)
        return;

    m_gl.glClear(GL_DEPTH_BUFFER_BIT);

    DrawParts();
}

void CRenderer2DLegacy::DrawParts() const
{
    const unsigned char renFlags = CSettings::GetInstance().GetRenderFlags();

    if(renFlags & CSettings::R_FLAPS)
    {
        DrawFlaps();
        m_gl.glClear(GL_DEPTH_BUFFER_BIT);
    }

    DrawGroups();

    if(renFlags & (CSettings::R_EDGES | CSettings::R_FOLDS))
    {
        DrawEdges();
    }
}

void CRenderer2DLegacy::DrawFlaps() const
{
    if(m_texFolds)
        m_texFolds->bind();

    const std::list<CMesh::SEdge>& edges = m_model->GetEdges();

    m_gl.glBegin(GL_QUADS);
    for(const CMesh::SEdge &e : edges)
    {
        if(!e.IsSnapped())
        {
            switch(e.GetFlapPosition())
            {
            case CMesh::SEdge::FP_LEFT:
                RenderFlap(e.GetTriangle(0), e.GetTriIndex(0));
                break;
            case CMesh::SEdge::FP_RIGHT:
                RenderFlap(e.GetTriangle(1), e.GetTriIndex(1));
                break;
            case CMesh::SEdge::FP_BOTH:
                RenderFlap(e.GetTriangle(0), e.GetTriIndex(0));
                RenderFlap(e.GetTriangle(1), e.GetTriIndex(1));
                break;
            case CMesh::SEdge::FP_NONE:
            default:
                break;
            }
        }
    }
    m_gl.glEnd();

    if(m_texFolds && m_texFolds->isBound())
        m_texFolds->release();
}

void CRenderer2DLegacy::DrawGroups() const
{
    const std::vector<glm::vec2> &uvs = m_model->GetUVCoords();

    m_gl.glBegin(GL_TRIANGLES);
    const auto &groups = m_model->GetGroups();
    for(auto it=groups.begin(); it!=groups.end(); ++it)
    {
        const CMesh::STriGroup &grp = *it;
        const std::list<CMesh::STriangle2D*>& grpTris = grp.GetTriangles();

        for(auto it2=grpTris.begin(), itEnd = grpTris.end(); it2!=itEnd; ++it2)
        {
            const CMesh::STriangle2D& tr2D = **it2;
            const glm::uvec4 &t = m_model->GetTriangles()[tr2D.ID()];

            BindTexture(t[3]);

            const glm::vec2 vertex1 = tr2D[0];
            const glm::vec2 vertex2 = tr2D[1];
            const glm::vec2 vertex3 = tr2D[2];

            const glm::vec2 &uv1 = uvs[t[0]];
            const glm::vec2 &uv2 = uvs[t[1]];
            const glm::vec2 &uv3 = uvs[t[2]];

            m_gl.glTexCoord2f(uv1[0], uv1[1]);
            m_gl.glVertex3f(vertex1[0], vertex1[1], -grp.GetDepth());

            m_gl.glTexCoord2f(uv2[0], uv2[1]);
            m_gl.glVertex3f(vertex2[0], vertex2[1], -grp.GetDepth());

            m_gl.glTexCoord2f(uv3[0], uv3[1]);
            m_gl.glVertex3f(vertex3[0], vertex3[1], -grp.GetDepth());
        }
    }
    m_gl.glEnd();

    UnbindTexture();
}

void CRenderer2DLegacy::DrawEdges() const
{
    const CSettings& sett = CSettings::GetInstance();
    const unsigned char renFlags = sett.GetRenderFlags();
    const float maxFlatAngle = (float)sett.GetFoldMaxFlatAngle();

    m_gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    m_gl.glEnable(GL_BLEND);

    if(m_texFolds)
        m_texFolds->bind();

    const std::list<CMesh::SEdge>& edges = m_model->GetEdges();

    m_gl.glBegin(GL_QUADS);
    for(const CMesh::SEdge &e : edges)
    {
        int foldType = (int)e.GetFoldType();
        if(foldType == CMesh::SEdge::FT_FLAT && e.IsSnapped())
            continue;

     if(e.HasTwoTriangles())
     {
         if(e.IsSnapped() && (renFlags & CSettings::R_FOLDS))
         {
             if(e.GetAngle() > maxFlatAngle)
             {
                RenderEdge(e.GetTriangle(0), e.GetTriIndex(0), foldType);
             }
         } else if(!e.IsSnapped() && (renFlags & CSettings::R_EDGES)) {
             RenderEdge(e.GetTriangle(0), e.GetTriIndex(0), CMesh::SEdge::FT_FLAT);
             RenderEdge(e.GetTriangle(1), e.GetTriIndex(1), CMesh::SEdge::FT_FLAT);
         }
     } else if(renFlags & CSettings::R_EDGES) {
         void *t = static_cast<void*>(e.GetAnyTriangle());
         int edge = e.GetAnyTriIndex();
         RenderEdge(t, edge, CMesh::SEdge::FT_FLAT);
     }
    }
    m_gl.glEnd();
    m_gl.glDisable(GL_BLEND);

    if(m_texFolds && m_texFolds->isBound())
        m_texFolds->release();
}

void CRenderer2DLegacy::RenderFlap(void *tr, int edge) const
{
    const CMesh::STriangle2D& t = *static_cast<CMesh::STriangle2D*>(tr);
    const CMesh::STriGroup *g = t.GetGroup();
    const float dep = g->GetDepth() + CMesh::STriGroup::GetDepthStep()*0.3f;
    const float dep2 = dep + CMesh::STriGroup::GetDepthStep()*0.15f;
    const glm::vec2 &v1 = t[edge];
    const glm::vec2 &v2 = t[(edge+1)%3];
    const glm::vec2 vN = t.GetNormal(edge) * 0.5f;

    float x[4];
    float y[4];
    if(t.IsFlapSharp(edge))
    {
        x[0] = v1.x;
        y[0] = v1.y;
        x[1] = 0.5f*v1.x + 0.5f*v2.x + vN.x;
        y[1] = 0.5f*v1.y + 0.5f*v2.y + vN.y;
        x[2] = v2.x;
        y[2] = v2.y;
        x[3] = 0.5f*v1.x + 0.5f*v2.x;
        y[3] = 0.5f*v1.y + 0.5f*v2.y;
    } else {
        x[0] = v1.x;
        y[0] = v1.y;
        x[1] = 0.9f*v1.x + 0.1f*v2.x + vN.x;
        y[1] = 0.9f*v1.y + 0.1f*v2.y + vN.y;
        x[2] = 0.1f*v1.x + 0.9f*v2.x + vN.x;
        y[2] = 0.1f*v1.y + 0.9f*v2.y + vN.y;
        x[3] = v2.x;
        y[3] = v2.y;
    }

    static const glm::mat2 rotMx90deg = glm::mat2(glm::vec2(0.0f, 1.0f),
                                                  glm::vec2(-1.0f, 0.0f));
    const float normalScaler = 0.015f * CSettings::GetInstance().GetLineWidth();

    //render inner part of flap
    m_gl.glTexCoord2f(0.0f, 0.8f); //white
    m_gl.glVertex3f(x[0], y[0], -dep2);
    m_gl.glVertex3f(x[1], y[1], -dep2);
    m_gl.glVertex3f(x[2], y[2], -dep2);
    m_gl.glVertex3f(x[3], y[3], -dep2);

    //render edges of flap
    m_gl.glTexCoord2f(0.0f, 0.1f); //black
    for(int i=0; i<4; i++)
    {
        int i2 = (i+1)%4;
        const float& x1 = x[i];
        const float& x2 = x[i2];
        const float& y1 = y[i];
        const float& y2 = y[i2];
        const glm::vec2 eN = glm::normalize(rotMx90deg * glm::vec2(x2-x1, y2-y1)) * normalScaler;

        m_gl.glVertex3f(x1 - eN.x, y1 - eN.y, -dep);
        m_gl.glVertex3f(x1 + eN.x, y1 + eN.y, -dep);
        m_gl.glVertex3f(x2 + eN.x, y2 + eN.y, -dep);
        m_gl.glVertex3f(x2 - eN.x, y2 - eN.y, -dep);
    }
}

void CRenderer2DLegacy::RenderEdge(void *tr, int edge, int foldType) const
{
    const CMesh::STriangle2D& t = *static_cast<CMesh::STriangle2D*>(tr);
    const CMesh::STriGroup *g = t.GetGroup();
    const glm::vec2 &v1 = t[edge];
    const glm::vec2 &v2 = t[(edge+1)%3];
    const glm::vec2 vN = t.GetNormal(edge) * 0.015f * CSettings::GetInstance().GetLineWidth();
    const float len = t.GetEdgeLen(edge) * (float)CSettings::GetInstance().GetStippleLoop();
    const float dep = g->GetDepth() - CMesh::STriGroup::GetDepthStep()*0.3f;

    float foldSelector = 1.0f;

    switch(foldType)
    {
    case CMesh::SEdge::FT_FLAT:
        foldSelector = 1.0f;
        break;
    case CMesh::SEdge::FT_VALLEY:
        foldSelector = 2.0f;
        break;
    case CMesh::SEdge::FT_MOUNTAIN:
        foldSelector = 3.0f;
        break;
    default: assert(false);
    }

    static const float oneForth = 1.0f/4.0f;

    m_gl.glTexCoord2f(0.0f, oneForth * (foldSelector - 1.0f) + 0.1f);
    m_gl.glVertex3f(v1.x - vN.x, v1.y - vN.y, -dep);

    m_gl.glTexCoord2f(0.0f, oneForth * foldSelector - 0.1f);
    m_gl.glVertex3f(v1.x + vN.x, v1.y + vN.y, -dep);

    m_gl.glTexCoord2f(len, oneForth * foldSelector - 0.1f);
    m_gl.glVertex3f(v2.x + vN.x, v2.y + vN.y, -dep);

    m_gl.glTexCoord2f(len, oneForth * (foldSelector - 1.0f) + 0.1f);
    m_gl.glVertex3f(v2.x - vN.x, v2.y - vN.y, -dep);
}

QImage CRenderer2DLegacy::DrawImageFromSheet(const glm::vec2 &pos) const
{
    const CSettings& sett = CSettings::GetInstance();

    const int papW = sett.GetPaperWidth();
    const int papH = sett.GetPaperHeight();
    const int fboW = (int)(papW * sett.GetResolutionScale());
    const int fboH = (int)(papH * sett.GetResolutionScale());

    m_gl.glViewport(0, 0, fboW, fboH);

    m_gl.glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    m_gl.glMatrixMode(GL_PROJECTION);
    m_gl.glPushMatrix();
    m_gl.glLoadIdentity();
    m_gl.glOrtho(-papW * 0.05f, papW * 0.05f, -papH * 0.05f, papH * 0.05f, 0.1f, 2000.0f);

    m_gl.glMatrixMode(GL_MODELVIEW);

    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setSamples(6);
    fboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
    fboFormat.setTextureTarget(GL_TEXTURE_2D_MULTISAMPLE);
    QOpenGLFramebufferObject fbo(fboW, fboH, fboFormat);
    if(!fbo.isValid())
    {
        throw std::logic_error("Failed to create framebuffer object");
    }
    fbo.bind();

    m_gl.glLoadIdentity();
    m_gl.glTranslatef(-pos.x - papW*0.05f, -pos.y - papH*0.05f, -1.0f);

    m_gl.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    DrawParts();

    QImage img(fbo.toImage());

    assert(fbo.release());

    m_gl.glViewport(0, 0, m_width, m_height);

    m_gl.glMatrixMode(GL_PROJECTION);
    m_gl.glPopMatrix();

    m_gl.glClearColor(0.7f, 0.7f, 0.7f, 0.7f);

    return img;
}

void CRenderer2DLegacy::BindTexture(unsigned id) const
{
    const bool renTexture = CSettings::GetInstance().GetRenderFlags() & CSettings::R_TEXTR;
    if(renTexture && m_boundTextureID != (int)id)
    {
        m_gl.glEnd();
        if(m_textures[id])
        {
            m_textures[id]->bind();
        } else if(m_boundTextureID >= 0 && m_textures[m_boundTextureID])
        {
            m_textures[m_boundTextureID]->release();
        }
        m_gl.glBegin(GL_TRIANGLES);
        m_boundTextureID = id;
    }
}

void CRenderer2DLegacy::UnbindTexture() const
{
    const bool renTexture = CSettings::GetInstance().GetRenderFlags() & CSettings::R_TEXTR;
    if(renTexture)
    {
        for(auto it=m_textures.begin(); it!=m_textures.end(); it++)
        {
            if(m_textures[it->first] && m_textures[it->first]->isBound())
            {
                m_textures[it->first]->release();
                break;
            }
        }
        m_boundTextureID = -1;
    }
}

void CRenderer2DLegacy::ClearTextures()
{
    m_boundTextureID = -1;
    IRenderer2D::ClearTextures();
}
