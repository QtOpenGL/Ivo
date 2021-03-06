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
#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <list>
#include <stdexcept>
#include <unordered_set>
#include <limits>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/norm.hpp>
#include <functional>
#include "mesh/mesh.h"
#include "mesh/command.h"
#include "io/utils.h"
#include "notification/hub.h"
#include "geometric/compgeom.h"

using glm::mat3;
using glm::vec2;
using glm::vec3;
using glm::sin;
using glm::cos;
using glm::min;
using glm::max;
using glm::radians;
using glm::sqrt;
using glm::inverse;
using glm::distance2;
using glm::transformation;

float CMesh::STriGroup::ms_depthStep = 1.0f;

CMesh::STriGroup::STriGroup() :
    m_position(vec2(0.0f,0.0f)),
    m_rotation(0.0f),
    m_matrix(1)
{
}

bool CMesh::STriGroup::AddTriangle(STriangle2D* tr, STriangle2D* referal)
{
    if(referal == nullptr)
    {
        m_tris.push_front(tr);
        tr->m_myGroup = this;
        mat3 id(1);
        tr->SetRelMx(id);
        for(int v=0; v<3; ++v)
        {
            const vec2 &vert = tr->m_vtxRT[v];
            m_toTopLeft[0] = min(m_toTopLeft[0], vert[0]);
            m_toTopLeft[1] = max(m_toTopLeft[1], vert[1]);
            m_toRightDown[0] = max(m_toRightDown[0], vert[0]);
            m_toRightDown[1] = min(m_toRightDown[1], vert[1]);
        }
        return true;
    }
    if(tr->m_myGroup == referal->m_myGroup)
        return false;

    //find adjacent edges
    int e1=-1, e2=-1;
    for(int i=0; i<3; ++i)
    {
        if(tr->m_edges[i]->m_left == referal ||
           tr->m_edges[i]->m_right == referal)
            e1 = i;
        if(referal->m_edges[i]->m_left == tr ||
           referal->m_edges[i]->m_right == tr)
            e2 = i;
    }
    assert(e1 > -1 && e2 > -1);
    STriangle2D backup = *tr;
    //rotate and translate tr to match referal's edge
    float trNewRotation = referal->m_rotation + 180.0f + tr->m_angleOY[e1] - referal->m_angleOY[e2];
    tr->SetRotation(trNewRotation);
    vec2 trNewPosition = referal->m_position - tr->m_vtxR[e1] + referal->m_vtxR[(e2+1)%3];
    tr->SetPosition(trNewPosition);

    //now check if tr overlaps any triangle in group
    for(auto it=m_tris.begin(); it!=m_tris.end(); ++it)
    {
        STriangle2D *toCheck = *it;
        if(toCheck == referal)
            continue;
        if(toCheck->Intersect(*tr))
        {
            *tr = backup; //cancel changes
            return false;
        }
    }
    tr->m_edges[e1]->m_snapped = true;
    m_tris.push_front(tr);
    mat3 id(1);
    tr->SetRelMx(id);
    tr->m_myGroup = this;
    for(int v=0; v<3; ++v)
    {
        const vec2 &vert = tr->m_vtxRT[v];
        m_toTopLeft[0] = min(m_toTopLeft[0], vert[0]);
        m_toTopLeft[1] = max(m_toTopLeft[1], vert[1]);
        m_toRightDown[0] = max(m_toRightDown[0], vert[0]);
        m_toRightDown[1] = min(m_toRightDown[1], vert[1]);
    }

    //check if other edges can be snapped
    for(int i=0; i<3; i++)
    {
        if(e1 == i)
            continue;
        if(!tr->m_edges[i]->HasTwoTriangles())
            continue;

        STriangle2D* otherTri = tr->m_edges[i]->GetOtherTriangle(tr);

        if(tr->GetGroup() != otherTri->GetGroup())
            continue;

        int i2 = tr->m_edges[i]->GetOtherTriIndex(tr);

        const vec2& tr1V2 = tr->m_vtxRT[i];
        const vec2& tr1V1 = tr->m_vtxRT[(i+1)%3];
        const vec2& tr2V1 = otherTri->m_vtxRT[i2];
        const vec2& tr2V2 = otherTri->m_vtxRT[(i2+1)%3];

        static const float epsilon = 0.001f;

        if(fabs(tr1V1.x - tr2V1.x) < epsilon &&
           fabs(tr1V1.y - tr2V1.y) < epsilon &&
           fabs(tr1V2.x - tr2V2.x) < epsilon &&
           fabs(tr1V2.y - tr2V2.y) < epsilon)
        {
            tr->m_edges[i]->SetSnapped(true);
        }
    }
    return true;
}

void CMesh::STriGroup::ResetBBoxVectors()
{
    m_toTopLeft   = vec2(std::numeric_limits<float>::max(),    std::numeric_limits<float>::lowest());
    m_toRightDown = vec2(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max());
}

void CMesh::STriGroup::RecalcBBoxVectors()
{
    ResetBBoxVectors();

    for(STriangle2D *t : m_tris)
        for(int v=0; v<3; ++v)
        {
            const vec2 &vert = t->m_vtxRT[v];
            m_toTopLeft[0] = min(m_toTopLeft[0], vert[0]);
            m_toTopLeft[1] = max(m_toTopLeft[1], vert[1]);
            m_toRightDown[0] = max(m_toRightDown[0], vert[0]);
            m_toRightDown[1] = min(m_toRightDown[1], vert[1]);
        }
}

void CMesh::STriGroup::SetRotation(float angle)
{
    m_rotation = angle;
    while(m_rotation >= 360.0f)
        m_rotation -= 360.0f;
    while(m_rotation < 0.0f)
        m_rotation += 360.0f;
    float rotRAD = radians(m_rotation);
    m_matrix = transformation(m_matrix[2], rotRAD);

    for(STriangle2D *t : m_tris)
        t->GroupHasTransformed(m_matrix);

    RecalcBBoxVectors();
}

void CMesh::STriGroup::SetPosition(float x, float y)
{
    m_toRightDown.x += x - m_position.x;
    m_toRightDown.y += y - m_position.y;
    m_toTopLeft.x += x - m_position.x;
    m_toTopLeft.y += y - m_position.y;
    m_matrix[2][0] = m_position[0] = x;
    m_matrix[2][1] = m_position[1] = y;
    for(STriangle2D *t : m_tris)
    {
        t->GroupHasTransformed(m_matrix);
    }
}

void CMesh::STriGroup::CentrateOrigin()
{
    m_position = vec2(0.0f, 0.0f);
    for(const auto tri : m_tris)
    {
        const STriangle2D& tr = *tri;
        m_position += tr.m_vtxRT[0];
        m_position += tr.m_vtxRT[1];
        m_position += tr.m_vtxRT[2];
    }
    m_position /= m_tris.size() * 3;

    float aabbHSideSQR = 0.0f;
    for(const auto tri : m_tris)
    {
        const STriangle2D& tr = *tri;
        for(int i=0; i<3; ++i)
        {
            float distanceSQR = distance2(m_position, tr.m_vtxRT[i]);

            if(distanceSQR > aabbHSideSQR)
                aabbHSideSQR = distanceSQR;
        }
    }
    m_aabbHSide = sqrt(aabbHSideSQR);

    float rotRAD = radians(m_rotation);
    m_matrix = transformation(m_position, rotRAD);
    mat3 pinv = inverse(m_matrix);

    for(STriangle2D *t : m_tris)
        t->SetRelMx(pinv);
}

void CMesh::STriGroup::AttachGroup(STriangle2D* tr2, int e2)
{
    NOTIFY(CMesh::GroupStructureChanging);

    assert(tr2 && e2 >= 0 && e2 <= 2);
    const STriGroup* grp = tr2->m_myGroup;

    m_tris.insert(m_tris.end(), grp->m_tris.begin(), grp->m_tris.end());

    for(STriangle2D*& t : m_tris)
        t->m_myGroup = this;

    RecalcBBoxVectors();
    CentrateOrigin();

    for(auto it = CMesh::g_Mesh->m_groups.begin(); it != CMesh::g_Mesh->m_groups.end(); ++it)
    {
        if(&(*it) == grp)
        {
            CMesh::g_Mesh->m_groups.erase(it);
            break;
        }
    }
    CMesh::g_Mesh->UpdateGroupDepth();
}

CIvoCommand* CMesh::STriGroup::GetJoinEdgeCmd(STriangle2D *tr, int e)
{
    assert(e >= 0 && e < 3 && tr);
    if(!tr->m_edges[e]->HasTwoTriangles()) return nullptr;

    //get triangle at the other side of edge 'e'
    STriangle2D *tr2 = tr->m_edges[e]->GetOtherTriangle(tr);

    if(!tr2) return nullptr;

    //get index of the other side of edge 'e'
    int e2 = tr->m_edges[e]->GetOtherTriIndex(tr);

    assert(e2 > -1);

    std::function<CIvoCommand*(STriangle2D*, STriangle2D*, int, int)>
            getSnapCommand = [this](STriangle2D* tr, STriangle2D* tr2, int e, int e2) -> CIvoCommand*
        {
            if(!tr || !tr2 || e < 0 || e2 < 0 || e > 2 || e2 > 2)
                return nullptr;
            if(tr->GetGroup() != tr2->GetGroup())
                return nullptr;
            if(tr->m_edges[e]->IsSnapped())
                return nullptr;

            const vec2& tr1V2 = tr->m_vtxRT[e];
            const vec2& tr1V1 = tr->m_vtxRT[(e+1)%3];
            const vec2& tr2V1 = tr2->m_vtxRT[e2];
            const vec2& tr2V2 = tr2->m_vtxRT[(e2+1)%3];

            static const float epsilon = 0.001f;

            if(fabs(tr1V1.x - tr2V1.x) < epsilon &&
               fabs(tr1V1.y - tr2V1.y) < epsilon &&
               fabs(tr1V2.x - tr2V2.x) < epsilon &&
               fabs(tr1V2.y - tr2V2.y) < epsilon)
            {
                CAtomicCommand cmdSnp(CT_SNAP_EDGE);
                cmdSnp.SetEdge(e2);
                cmdSnp.SetTriangle(tr2);

                CIvoCommand* cmd = new CIvoCommand();
                cmd->AddAction(cmdSnp);

                return cmd;
            }
            return nullptr;
        };

    CIvoCommand* snapCmd = getSnapCommand(tr, tr2, e, e2);
    if(snapCmd)
    {
        return snapCmd;
    }
    if(tr->GetGroup() == tr2->GetGroup())
        return nullptr;

    STriGroup *grp = tr2->m_myGroup;

    CAtomicCommand cmdRot(CT_ROTATE);
    CAtomicCommand cmdMov(CT_MOVE);
    CAtomicCommand cmdAtt(CT_JOIN_GROUPS);
    CAtomicCommand cmdSnp(CT_SNAP_EDGE);
    cmdRot.SetTriangle(tr2);
    cmdMov.SetTriangle(tr2);
    cmdSnp.SetTriangle(tr2);
    cmdSnp.SetEdge(e2);
    cmdAtt.SetTriangle(tr);
    cmdAtt.SetEdge(e);

    float oldrot = grp->m_rotation;
    float newRotation = grp->m_rotation - tr2->m_rotation + tr2->m_angleOY[e2] + 180.0f - tr->m_angleOY[e] + tr->m_rotation;
    cmdRot.SetRotation(newRotation - grp->GetRotation());
    grp->SetRotation(newRotation);

    vec2 newPos = tr->m_vtxRT[e] - tr2->m_vtxRT[(e2+1)%3]/*(mat2(matrix) * tr2->m_vtx[(e2+1)%3] + vec2(matrix[2][0], matrix[2][1]))*/ + grp->m_position;
    cmdMov.SetTranslation(newPos - grp->GetPosition());
    grp->SetRotation(oldrot);

    CIvoCommand* cmd = new CIvoCommand();
    cmd->AddAction(cmdRot);
    cmd->AddAction(cmdMov);
    cmd->AddAction(cmdSnp);
    cmd->AddAction(cmdAtt);

    //if there are triangles, that can be potentially snapped...
    if(m_tris.size() > 1 || grp->m_tris.size() > 1)
    {
        //apply current command to update positions
        cmd->redo();// 'grp' is no longer valid

        for(STriangle2D* tri : m_tris)
        {
            for(int e=0; e<3; e++)
            {
                STriangle2D* tri2 = tri->m_edges[e]->GetOtherTriangle(tri);
                int e2 = tri->m_edges[e]->GetOtherTriIndex(tri);

                //check if triangles can be snapped
                CIvoCommand* snapCmd = getSnapCommand(tri, tri2, e, e2);
                if(snapCmd)
                {
                    //if so, do this to make sure it's done only once
                    snapCmd->redo();
                    cmd->AddAction(std::move(*snapCmd));
                    delete snapCmd;
                }
            }
        }

        //return back to the beginning
        cmd->undo();
    }

    return cmd;
}

SAABBox2D CMesh::STriGroup::GetAABBox() const
{
    return SAABBox2D(m_toRightDown, m_toTopLeft);
}

void CMesh::STriGroup::JoinEdge(STriangle2D *tr, int e)
{
    CIvoCommand* cmd = GetJoinEdgeCmd(tr, e);

    if(!cmd)
        return;

    CMesh::g_Mesh->m_undoStack.push(cmd);
    CMesh::g_Mesh->UpdateGroupDepth();
}

void CMesh::STriGroup::BreakGroup(STriangle2D *tr2, int e2)
{
    NOTIFY(CMesh::GroupStructureChanging);

    assert(tr2 && e2 >= 0 && e2 <= 2);
    if(!tr2->m_edges[e2]->HasTwoTriangles()) return;

    STriangle2D *tr = tr2->m_edges[e2]->GetOtherTriangle(tr2);
    int e = tr2->m_edges[e2]->GetOtherTriIndex(tr2);


    //all triangles, connected to tr without tr2
    std::list<STriangle2D*> trAndCompany;

    trAndCompany.push_back(tr);
    std::unordered_set<STriangle2D*> currLevel; //of breadth-first search
    for(int i=0; i<3; ++i)
    {
        if(i != e && tr->m_edges[i]->m_snapped && tr->m_edges[i]->HasTwoTriangles())
        {
            currLevel.insert(tr->m_edges[i]->GetOtherTriangle(tr));
        }
    }
    while(!currLevel.empty())
    {
        std::unordered_set<STriangle2D*> nextLevel;
        for(STriangle2D* t : currLevel)
        {
            for(int i=0; i<3; ++i)
            {
                STriangle2D *nbs = nullptr;
                if(t->m_edges[i]->HasTwoTriangles())
                    nbs = t->m_edges[i]->GetOtherTriangle(t);
                if(nbs &&
                   t->m_edges[i]->m_snapped &&
                   currLevel.find(nbs) == currLevel.end() &&
                   std::find(trAndCompany.begin(), trAndCompany.end(), nbs) == trAndCompany.end())
                    nextLevel.insert(nbs);
            }
        }
        trAndCompany.insert(trAndCompany.begin(), currLevel.begin(), currLevel.end());
        currLevel = nextLevel;
    }

    CMesh::g_Mesh->m_groups.emplace_back();
    STriGroup &newGroup = CMesh::g_Mesh->m_groups.back();

    newGroup.ResetBBoxVectors();
    for(STriangle2D*& t : m_tris)
    {
        if(std::find(trAndCompany.begin(), trAndCompany.end(), t) == trAndCompany.end())
        {
            newGroup.AddTriangle(t, nullptr);
        }
    }
    newGroup.CentrateOrigin();

    ResetBBoxVectors();
    m_tris.clear();

    for(STriangle2D*& t : trAndCompany)
    {
        AddTriangle(t, nullptr);
    }

    CentrateOrigin();
}

CIvoCommand* CMesh::STriGroup::GetBreakEdgeCmd(STriangle2D *tr, int e)
{
    assert(e >= 0 && e < 3 && tr);
    if(!tr->m_edges[e]->HasTwoTriangles()) return nullptr;

    //get triangle at the other side of edge 'e'
    STriangle2D *tr2 = tr->m_edges[e]->GetOtherTriangle(tr);
    if(!tr2) return nullptr;
    //get index of the other side of edge 'e'
    int e2 = tr->m_edges[e]->GetOtherTriIndex(tr);
    assert(e2 > -1);

    //all triangles, connected to tr without tr2
    std::list<STriangle2D*> trAndCompany;
    trAndCompany.push_back(tr);

    std::unordered_set<STriangle2D*> currLevel; //of breadth-first search
    for(int i=0; i<3; ++i)
    {
        if(i != e && tr->m_edges[i]->m_snapped && tr->m_edges[i]->HasTwoTriangles())
        {
            currLevel.insert(tr->m_edges[i]->GetOtherTriangle(tr));
        }
    }

    //if cutOff == false, then we can reach tr2 from tr by series of edges without 'e', and we cannot split the group yet
    bool cutOff = true;
    while(!currLevel.empty())
    {
        std::unordered_set<STriangle2D*> nextLevel;
        for(STriangle2D* t : currLevel)
        {
            for(int i=0; i<3; ++i)
            {
                STriangle2D *nbs = nullptr;
                if(t->m_edges[i]->HasTwoTriangles())
                    nbs = t->m_edges[i]->GetOtherTriangle(t);
                if(nbs &&
                   t->m_edges[i]->m_snapped &&
                   currLevel.find(nbs) == currLevel.end() &&
                   std::find(trAndCompany.begin(), trAndCompany.end(), nbs) == trAndCompany.end())
                    nextLevel.insert(nbs);
            }
        }
        if(nextLevel.find(tr2) != nextLevel.end())
        {
            cutOff = false;
            break;
        }
        trAndCompany.insert(trAndCompany.begin(), currLevel.begin(), currLevel.end());
        currLevel = nextLevel;
    }

    CIvoCommand* cmd = new CIvoCommand();

    CAtomicCommand cmdBrk(CT_BREAK_EDGE);
    cmdBrk.SetTriangle(tr);
    cmdBrk.SetEdge(e);

    cmd->AddAction(cmdBrk);

    if(cutOff)
    {
        CAtomicCommand cmdBGrp(CT_BREAK_GROUP);
        CAtomicCommand cmdMv1(CT_MOVE);
        CAtomicCommand cmdMv2(CT_MOVE);
        cmdBGrp.SetTriangle(tr2);
        cmdBGrp.SetEdge(e2);
        cmdMv1.SetTriangle(tr);
        cmdMv2.SetTriangle(tr2);

        vec2 oldTRN = tr->m_normR[e];
        vec2 oldTR2V0 = tr2->m_vtxRT[0];
        vec2 oldTRV0 = tr->m_vtxRT[0];

        STriGroup &newGroup = CMesh::g_Mesh->m_groups.back();
        vec2 newPos = newGroup.m_position + oldTRN + oldTR2V0 - tr2->m_vtxRT[0];
        cmdMv2.SetTranslation(newPos - newGroup.GetPosition());

        newPos = m_position - oldTRN + oldTRV0 - tr->m_vtxRT[0];
        cmdMv1.SetTranslation(newPos - GetPosition());

        cmd->AddAction(cmdBGrp);
        cmd->AddAction(cmdMv1);
        cmd->AddAction(cmdMv2);
    }

    return cmd;
}

void CMesh::STriGroup::BreakEdge(STriangle2D *tr, int e)
{
    CIvoCommand* cmd = GetBreakEdgeCmd(tr, e);

    if(!cmd)
        return;

    CMesh::g_Mesh->m_undoStack.push(cmd);
    CMesh::g_Mesh->UpdateGroupDepth();
}

QJsonObject CMesh::STriGroup::Serialize() const
{
    assert(CMesh::g_Mesh);
    std::vector<int> trInds(m_tris.size());
    int i = 0;
    for(const STriangle2D* tr : m_tris)
        trInds[i++] = static_cast<int>(tr - &(CMesh::g_Mesh->m_tri2D[0]));

    QJsonObject grpObject;
    grpObject.insert("triangleIndices", ToJSON(trInds));
    grpObject.insert("toTopLeft", ToJSON(m_toTopLeft));
    grpObject.insert("toRightDown", ToJSON(m_toRightDown));
    grpObject.insert("aabbHSide", ToJSON(m_aabbHSide));
    grpObject.insert("position", ToJSON(m_position));
    grpObject.insert("rotation", ToJSON(m_rotation));
    grpObject.insert("matrix", ToJSON(m_matrix));
    return grpObject;
}

void CMesh::STriGroup::Deserialize(const QJsonObject& obj)
{
    std::vector<int> trInds;
    FromJSON(obj["triangleIndices"], trInds);
    for(std::size_t i=0; i<trInds.size(); ++i)
    {
        if(trInds[i] >= static_cast<int>(CMesh::g_Mesh->m_tri2D.size()))
            throw std::runtime_error("File corrupted: triangle index in group is out of range!");

        m_tris.push_back(&(CMesh::g_Mesh->m_tri2D[trInds[i]]));
        m_tris.back()->m_myGroup = this;
    }

    FromJSON(obj["toTopLeft"], m_toTopLeft);
    FromJSON(obj["toRightDown"], m_toRightDown);
    FromJSON(obj["aabbHSide"], m_aabbHSide);
    FromJSON(obj["position"], m_position);
    FromJSON(obj["rotation"], m_rotation);
    FromJSON(obj["matrix"], m_matrix);
}

void CMesh::STriGroup::Scale(const float scale)
{
    for(STriangle2D* tri : m_tris)
        tri->Scale(scale);

    RecalcBBoxVectors();
    CentrateOrigin();
}

const float& CMesh::STriGroup::GetDepth() const
{
    return m_depth;
}

float CMesh::STriGroup::GetDepthStep()
{
    return ms_depthStep;
}

const float& CMesh::STriGroup::GetAABBHalfSide() const
{
    return m_aabbHSide;
}

const std::list<CMesh::STriangle2D*>& CMesh::STriGroup::GetTriangles() const
{
    return m_tris;
}
