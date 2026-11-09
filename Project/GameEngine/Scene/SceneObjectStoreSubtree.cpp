#include "GameEngine/pch.h"
#include "GameEngine/Scene/SceneObjectStore.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Object/Object.h"
#include <algorithm>
#include <functional>
#include <stdexcept>

namespace GameEngine {
std::vector<std::string> SceneObjectStore::GetSubtreeIds(
   const std::string& rootId, const std::vector<std::string>& order) const {
   if (!FindById(rootId)) {
      return {};
   }

   auto objects = GetObjects();
   std::unordered_map<std::string, size_t> ranks;

   for (size_t index = 0; index < order.size(); ++index) {
      ranks.try_emplace(order[index], index);
   }

   std::stable_sort(objects.begin(), objects.end(), [&](const Object* a, const Object* b) {
      const auto left = ranks.find(a->GetEntityId());
      const auto right = ranks.find(b->GetEntityId());

      if ((left != ranks.end()) != (right != ranks.end())) {
         return left != ranks.end();
      }

      return left != ranks.end() && left->second < right->second;
   });

   std::unordered_map<std::string, std::vector<std::string>> children;

   for (const auto* object : objects) {
      children[object->GetParentEntityId()].push_back(object->GetEntityId());
   }

   std::vector<std::string> result;
   std::unordered_set<std::string> visited;
   std::function<void(const std::string&)> visit = [&](const std::string& id) {
      if (!visited.insert(id).second) {
         return;
      }

      result.push_back(id);

      for (const auto& child : children[id]) {
         visit(child);
      }
   };

   visit(rootId);
   return result;
}

nlohmann::json SceneObjectStore::CaptureSubtree(
   const std::string& rootId, std::string& error, const std::vector<std::string>& order) const {
   try {
      const auto ids = GetSubtreeIds(rootId, order);

      if (ids.empty()) {
         throw std::runtime_error("Subtree root does not exist");
      }

      nlohmann::json snapshot{{"rootId", rootId}, {"objects", nlohmann::json::array()}};

      for (const auto& id : ids) {
         snapshot["objects"].push_back(SerializeObjectState(FindById(id)));
      }

      error.clear();
      return snapshot;
   } catch (const std::exception& failure) {
      error = failure.what();
      return {};
   }
}

std::string SceneObjectStore::RestoreSubtree(const nlohmann::json& snapshot, bool freshIds, std::string& error) {
   std::vector<std::string> created;

   try {
      const auto rootId = snapshot.at("rootId").get<std::string>();
      const auto& objects = snapshot.at("objects");

      if (rootId.empty() || !objects.is_array()) {
         throw std::runtime_error("Invalid subtree snapshot");
      }

      std::unordered_map<std::string, std::string> parents;
      std::unordered_map<std::string, std::string> ids;

      for (const auto& object : objects) {
         const auto id = object.at("id").get<std::string>();

         if (id.empty() || !parents.emplace(id, object.value("parentId", "")).second) {
            throw std::runtime_error("Duplicate or empty subtree ID");
         }

         if (!freshIds && Object::FindByEntityId(id)) {
            throw std::runtime_error("Subtree ID is already in use");
         }
      }

      if (!parents.contains(rootId)) {
         throw std::runtime_error("Subtree root is missing");
      }

      const auto& rootParent = parents.at(rootId);

      if (!rootParent.empty() && (parents.contains(rootParent) || !FindById(rootParent))) {
         throw std::runtime_error("Invalid subtree root parent");
      }

      for (const auto& [id, parent] : parents) {
         auto current = id;
         std::unordered_set<std::string> visited;

         while (current != rootId) {
            if (!visited.insert(current).second || !parents.contains(current)) {
               throw std::runtime_error("Invalid subtree hierarchy");
            }

            current = parents.at(current);
         }
      }

      // 親を外して全IDを確保した後に接続する。別インスタンスの元IDを親として参照しない。
      for (auto object : objects) {
         const auto oldId = object.at("id").get<std::string>();
         object.erase("parentId");

         for (auto& component : object["components"]) {
            if (component.value("typeName", "") == "TransformComponent") {
               component["data"]["parentEntityId"] = "";
            }
         }

         if (freshIds) {
            object.erase("id");
         }

         auto* restored = RestoreObject(object);

         if (!restored) {
            throw std::runtime_error("Failed to restore subtree object");
         }

         const auto newId = restored->GetEntityId();
         created.push_back(newId);

         if (!freshIds && newId != oldId) {
            throw std::runtime_error("Subtree ID could not be restored");
         }

         ids.emplace(oldId, newId);
      }

      std::vector<Object*> restoredObjects;

      for (const auto& [id, parent] : parents) {
         auto* object = FindById(ids.at(id));
         const auto parentId = id == rootId ? rootParent : ids.at(parent);

         if (!object || !object->SetParentEntityId(parentId)) {
            throw std::runtime_error("Failed to connect subtree hierarchy");
         }

         restoredObjects.push_back(object);
      }

      world_.ResolveReferences(restoredObjects);
      error.clear();
      return ids.at(rootId);
   } catch (const std::exception& failure) {
      for (auto it = created.rbegin(); it != created.rend(); ++it) {
         DeleteObject(*it);
      }

      error = failure.what();
      return {};
   }
}

bool SceneObjectStore::DeleteSubtree(const std::string& rootId) {
   const auto ids = GetSubtreeIds(rootId);

   for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
      DeleteObject(*it);
   }

   return !ids.empty();
}
}
