/*
Copyright(c) 2015-2025 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= INCLUDES ======================
#include "pch.h"
#include "Entity.h"
#include "Components/Camera.h"
#include "Components/Light.h"
#include "Components/Physics.h"
#include "Components/AudioSource.h"
#include "Components/Terrain.h"
SP_WARNINGS_OFF
#include "../IO/pugixml.hpp"
SP_WARNINGS_ON
//=================================

//= NAMESPACES ===============
using namespace std;
using namespace spartan::math;
//============================

namespace spartan
{
    namespace
    {
        // input is an entity, output is a clone of that entity (descendant entities are not cloned)
        shared_ptr<Entity> clone_entity(Entity* entity)
        {
            // clone basic properties
            shared_ptr<Entity> clone = World::CreateEntity();
            clone->SetObjectId(SpartanObject::GenerateObjectId());
            clone->SetObjectName(entity->GetObjectName());
            clone->SetActive(entity->GetActive());
            clone->SetPosition(entity->GetPositionLocal());
            clone->SetRotation(entity->GetRotationLocal());
            clone->SetScale(entity->GetScaleLocal());

            // clone all the components
            for (shared_ptr<Component> component_original : entity->GetAllComponents())
            {
                if (component_original != nullptr)
                {
                    // component
                    Component* component_clone = clone->AddComponent(component_original->GetType());

                    // component's properties
                    component_clone->SetAttributes(component_original->GetAttributes());
                }
            }

            return clone;
        };

        // input is an entity, output is a clone of that entity (descendant entities are cloned)
        shared_ptr<Entity> clone_entity_and_descendants(Entity* entity)
        {
            shared_ptr<Entity> clone_self = clone_entity(entity);

            // clone children make them call this lambda
            for (Entity* child_transform : entity->GetChildren())
            {
                shared_ptr<Entity> clone_child = clone_entity_and_descendants(child_transform);
                clone_child->SetParent(clone_self);
            }

            return clone_self;
        };
    }

    Entity::Entity()
    {
        m_object_name = "Entity";
        m_is_active   = true;

        m_components.fill(nullptr);
    }

    Entity::~Entity()
    {
        m_components.fill(nullptr);
    }

    void Entity::Initialize()
    {
        UpdateTransform();
    }

    shared_ptr<Entity> Entity::Clone()
    {
        return clone_entity_and_descendants(this);
    }

    void Entity::OnStart()
    {
        for (shared_ptr<Component> component : m_components)
        {
            if (component)
            {
                component->OnStart();
            }
        }
    }

    void Entity::OnStop()
    {
        for (shared_ptr<Component> component : m_components)
        {
            if (component)
            {
                component->OnStop();
            }
        }
    }

    void Entity::Tick()
    {
        for (shared_ptr<Component>& component : m_components)
        {
            if (component)
            {
                component->OnTick();
            }
        }

        m_time_since_last_transform_sec += static_cast<float>(Timer::GetDeltaTimeSec());
    }

    void Entity::Save(pugi::xml_node& node)
    {
        // self
        {
            node.append_attribute("name")      = m_object_name.c_str();
            node.append_attribute("id")        = m_object_id;
            node.append_attribute("active")    = m_is_active;
            
            {
                std::stringstream ss;
                ss << m_position_local.x << " " << m_position_local.y << " " << m_position_local.z;
                node.append_attribute("position") = ss.str().c_str();
            }

            {
                std::stringstream ss;
                ss << m_rotation_local.x << " " << m_rotation_local.y << " " << m_rotation_local.z << " " << m_rotation_local.w;
                node.append_attribute("rotation") = ss.str().c_str();
            }

            {
                std::stringstream ss;
                ss << m_scale_local.x << " " << m_scale_local.y << " " << m_scale_local.z;
                node.append_attribute("scale") = ss.str().c_str();
            }

            // components
            {

            }
        }

        // children
        for (Entity* child : m_children)
        {
            pugi::xml_node child_node = node.append_child("Entity");
            child->Save(child_node);
        }
    }

    void Entity::Load(pugi::xml_node& node)
    {
        // self
        {
            m_is_active   = node.attribute("active").as_bool();
            m_object_id   = node.attribute("id").as_ullong();
            m_object_name = node.attribute("name").as_string();

            {
                std::string pos_str = node.attribute("position").as_string();
                std::stringstream ss(pos_str);
                ss >> m_position_local.x >> m_position_local.y >> m_position_local.z;
            }

            {
                std::string rot_str = node.attribute("rotation").as_string();
                std::stringstream ss(rot_str);
                ss >> m_rotation_local.x >> m_rotation_local.y >> m_rotation_local.z >> m_rotation_local.w;
            }

            {
                std::string scale_str = node.attribute("scale").as_string();
                std::stringstream ss(scale_str);
                ss >> m_scale_local.x >> m_scale_local.y >> m_scale_local.z;
            }

            // components
            {

            }
        }

        // children
        for (pugi::xml_node child_node = node.child("Entity"); child_node; child_node = child_node.next_sibling("Entity"))
        {
            shared_ptr<Entity> child = World::CreateEntity();
            child->Load(child_node);
            child->SetParent(World::GetEntityById(GetObjectId()));
        }
    }

    bool Entity::GetActive() const
    {
        if (shared_ptr<Entity> parent = GetParent())
        {
            return m_is_active && parent->GetActive();
        }

        return m_is_active;
    }
    
    Component* Entity::AddComponent(const ComponentType type)
    {
        Component* component = nullptr;

        switch (type)
        {
            case ComponentType::AudioSource: component = static_cast<Component*>(AddComponent<AudioSource>()); break;
            case ComponentType::Camera:      component = static_cast<Component*>(AddComponent<Camera>());      break;
            case ComponentType::Light:       component = static_cast<Component*>(AddComponent<Light>());       break;
            case ComponentType::Renderable:  component = static_cast<Component*>(AddComponent<Renderable>());  break;
            case ComponentType::Physics:     component = static_cast<Component*>(AddComponent<Physics>());     break;
            case ComponentType::Terrain:     component = static_cast<Component*>(AddComponent<Terrain>());     break;
            default:                         component = nullptr;                                              break;
        }

        SP_ASSERT(component != nullptr);

        return component;
    }

    void Entity::RemoveComponentById(const uint64_t id)
    {
        for (shared_ptr<Component>& component : m_components)
        {
            if (component)
            {
                if (id == component->GetObjectId())
                {
                    component->OnRemove();
                    component = nullptr;
                    break;
                }
            }
        }

        World::Resolve();
    }

    void Entity::UpdateTransform()
    {
        // compute local transform
        m_matrix_local = Matrix(m_position_local, m_rotation_local, m_scale_local);

        // compute world transform
        if (!m_parent.expired())
        {
            m_matrix = m_matrix_local * m_parent.lock()->GetMatrix();
        }
        else
        {
            m_matrix = m_matrix_local;
        }

        // mark update
        m_time_since_last_transform_sec = 0.0f;

        // update children
        for (Entity* child : m_children)
        {
            child->UpdateTransform();
        }

        // update directions
        {
            // z
            m_forward  = GetRotation() * Vector3::Forward;
            m_backward = -m_forward;
            // y
            m_up       = GetRotation() * Vector3::Up;
            m_down     = -m_up;
            // x
            m_right    = GetRotation() * Vector3::Right;
            m_left     = -m_right;
        }
    }

    void Entity::SetPosition(const Vector3& position)
    {
        if (GetPosition() == position)
            return;

        SetPositionLocal(!HasParent() ? position : position * GetParent()->GetMatrix().Inverted());
    }

    void Entity::SetPositionLocal(const Vector3& position)
    {
        if (m_position_local == position)
            return;

        m_position_local = position;
        UpdateTransform();
    }

    void Entity::SetRotation(const Quaternion& rotation)
    {
        if (GetRotation() == rotation)
            return;

        SetRotationLocal(!HasParent() ? rotation : rotation * GetParent()->GetRotation().Inverse());
    }

    void Entity::SetRotationLocal(const Quaternion& rotation)
    {
        if (m_rotation_local == rotation)
            return;

        m_rotation_local = rotation;
        UpdateTransform();
    }

    void Entity::SetScale(const Vector3& scale)
    {
        if (GetScale() == scale)
            return;

        SetScaleLocal(!HasParent() ? scale : scale / GetParent()->GetScale());
    }

    void Entity::SetScaleLocal(const Vector3& scale)
    {
        if (m_scale_local == scale)
            return;

        m_scale_local = scale;

        // a scale of 0 will cause a division by zero when decomposing the world transform matrix
        m_scale_local.x = (m_scale_local.x == 0.0f) ? numeric_limits<float>::min() : m_scale_local.x;
        m_scale_local.y = (m_scale_local.y == 0.0f) ? numeric_limits<float>::min() : m_scale_local.y;
        m_scale_local.z = (m_scale_local.z == 0.0f) ? numeric_limits<float>::min() : m_scale_local.z;

        UpdateTransform();
    }

    void Entity::Translate(const Vector3& delta)
    {
        if (!HasParent())
        {
            SetPositionLocal(m_position_local + delta);
        }
        else
        {
            SetPositionLocal(m_position_local + GetParent()->GetMatrix().Inverted() * delta);
        }
    }

    void Entity::Rotate(const Quaternion& delta)
    {
        if (!HasParent())
        {
            SetRotationLocal((delta * m_rotation_local).Normalized());
        }
        else
        {
            SetRotationLocal(delta * m_rotation_local * GetRotation().Inverse() * delta * GetRotation());
        }
    }

    Entity* Entity::GetChildByIndex(const uint32_t index)
    {
        if (!HasChildren() || index >= GetChildrenCount())
            return nullptr;

        return m_children[index];
    }

    Entity* Entity::GetChildByName(const string& name)
    {
        for (Entity* child : m_children)
        {
            if (child->GetObjectName() == name)
                return child;
        }

        return nullptr;
    }

    void Entity::SetParent(weak_ptr<Entity> new_parent_in)
    {
        lock_guard lock(m_mutex_parent);

        shared_ptr<Entity> new_parent = new_parent_in.lock();
        shared_ptr<Entity> parent     = m_parent.lock();

        if (new_parent)
        {
            // early exit if the parent is this entity
            if (GetObjectId() == new_parent->GetObjectId())
                return;
        
            // early exit if the parent is already set
            if (parent && parent->GetObjectId() == new_parent->GetObjectId())
                return;
        
            // if the new parent is a descendant of this transform (e.g. dragging and dropping an entity onto one of it's children)
            if (new_parent->IsDescendantOf(this))
            {
                for (Entity* child : m_children)
                {
                    child->m_parent = m_parent; // directly setting parent
                    child->UpdateTransform();   // update transform if needed
                }
        
                m_children.clear();
            }
        }
        
        // remove the this as a child from the existing parent
        if (parent)
        {
            bool update_child_with_null_parent = false;
            parent->RemoveChild(this, update_child_with_null_parent);
        }
        
        // add this is a child to new parent
        if (new_parent)
        {
            new_parent->AddChild(this);
        }

        m_parent = new_parent_in;
        UpdateTransform();
    }

    void Entity::AddChild(Entity* child)
    {
        SP_ASSERT(child != nullptr);
        lock_guard lock(m_mutex_children);

        // ensure that the child is not this transform
        if (child->GetObjectId() == GetObjectId())
            return;

        // if this is not already a child, add it
        if (!(find(m_children.begin(), m_children.end(), child) != m_children.end()))
        {
            m_children.emplace_back(child);
        }
    }

    void Entity::RemoveChild(Entity* child, bool update_child_with_null_parent)
    {
        SP_ASSERT(child != nullptr);

        // ensure the transform is not itself
        if (child->GetObjectId() == GetObjectId())
            return;

        lock_guard lock(m_mutex_children);

        // remove the child
        m_children.erase(remove_if(m_children.begin(), m_children.end(), [child](Entity* vec_transform) { return vec_transform->GetObjectId() == child->GetObjectId(); }), m_children.end());

        // remove the child's parent
        if (update_child_with_null_parent)
        {
            shared_ptr<Entity> null = nullptr;
            child->SetParent(null);
        }
    }

    // searches the entire hierarchy, finds any children and saves them in m_children
    // this is a recursive function, the children will also find their own children and so on
    void Entity::AcquireChildren()
    {
        lock_guard lock(m_mutex_children);
        m_children.clear();
        m_children.shrink_to_fit();

        const vector<shared_ptr<Entity>>& entities = World::GetEntities();
        for (const shared_ptr<Entity>& possible_child : entities)
        {
            if (!possible_child || !possible_child->HasParent() || possible_child->GetObjectId() == GetObjectId())
                continue;

            // if it's parent matches this transform
            if (possible_child->GetParent()->GetObjectId() == GetObjectId())
            {
                // welcome home son
                m_children.emplace_back(possible_child.get());

                // make the child do the same thing all over, essentially resolving the entire hierarchy
                possible_child->AcquireChildren();
            }
        }
    }

    bool Entity::IsDescendantOf(Entity* transform) const
    {
        SP_ASSERT(transform != nullptr);

        if (m_parent.expired())
            return false;

        if (m_parent.lock()->GetObjectId() == transform->GetObjectId())
            return true;

        for (Entity* child : transform->GetChildren())
        {
            if (IsDescendantOf(child))
                return true;
        }

        return false;
    }

    void Entity::GetDescendants(vector<Entity*>* descendants)
    {
        for (Entity* child : m_children)
        {
            descendants->emplace_back(child);

            if (child->HasChildren())
            {
                child->GetDescendants(descendants);
            }
        }
    }

    Entity* Entity::GetDescendantByName(const string& name)
    {
        vector<Entity*> descendants;
        GetDescendants(&descendants);

        for (Entity* entity : descendants)
        {
            if (entity->GetObjectName() == name)
                return entity;
        }

        return nullptr;
    }

    Matrix Entity::GetParentTransformMatrix() const
    {
        return HasParent() ? GetParent()->GetMatrix() : Matrix::Identity;
    }
}
