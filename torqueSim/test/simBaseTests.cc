//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/platformString.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/memStream.h"
#include "sim/simBase.h"
#include <catch2/catch.hpp>

#include <atomic>
#include <array>
#include <cstring>
#include <string>
#include <vector>

class TestSimObject : public SimObject
{
   typedef SimObject Parent;

public:
   S32 mTestValue;
   U32 mOnAddCount;
   U32 mOnRemoveCount;
   U32 mOnGroupAddCount;
   U32 mOnGroupRemoveCount;
   U32 mOnNameChangeCount;
   U32 mOnStaticModifiedCount;
   U32 mOnDeleteNotifyCount;
   U32 mConsoleCallCount;
   S32 mLastProcessArgc;
   std::string mLastProcessArg0;
   std::string mLastName;
   std::string mLastStaticSlot;
   std::string mLastStaticValue;

   TestSimObject()
      : mTestValue(0),
        mOnAddCount(0),
        mOnRemoveCount(0),
        mOnGroupAddCount(0),
        mOnGroupRemoveCount(0),
        mOnNameChangeCount(0),
        mOnStaticModifiedCount(0),
        mOnDeleteNotifyCount(0),
        mConsoleCallCount(0),
        mLastProcessArgc(-1)
   {
   }

   static void initPersistFields()
   {
      Parent::initPersistFields();
      addField("testValue", TypeS32, Offset(mTestValue, TestSimObject));
   }

   bool onAdd() override
   {
      ++mOnAddCount;
      return Parent::onAdd();
   }

   void onRemove() override
   {
      ++mOnRemoveCount;
      Parent::onRemove();
   }

   void onGroupAdd() override
   {
      ++mOnGroupAddCount;
      Parent::onGroupAdd();
   }

   void onGroupRemove() override
   {
      ++mOnGroupRemoveCount;
      Parent::onGroupRemove();
   }

   void onNameChange(const char* name) override
   {
      ++mOnNameChangeCount;
      mLastName = name ? name : "";
      Parent::onNameChange(name);
   }

   void onStaticModified(const char* slotName, const char* newValue) override
   {
      ++mOnStaticModifiedCount;
      mLastStaticSlot = slotName ? slotName : "";
      mLastStaticValue = newValue ? newValue : "";
      Parent::onStaticModified(slotName, newValue);
   }

   void onDeleteNotify(SimObject* object) override
   {
      ++mOnDeleteNotifyCount;
      Parent::onDeleteNotify(object);
   }

   bool processArguments(S32 argc, const char** argv) override
   {
      mLastProcessArgc = argc;
      mLastProcessArg0 = (argc > 0 && argv && argv[0]) ? argv[0] : "";
      return Parent::processArguments(argc, argv);
   }

   DECLARE_CONOBJECT(TestSimObject);
};

IMPLEMENT_CONOBJECT(TestSimObject);

ConsoleMethod(TestSimObject, bumpCounter, void, 2, 2, "")
{
   object->mConsoleCallCount++;
}

static std::atomic<U32> gNextTestObjectId{100000};

static std::string uniqueName(const char* prefix)
{
   return std::string(prefix) + "_" + std::to_string(gNextTestObjectId.fetch_add(1));
}

template <class T, class... Args>
static T* createRegisteredObject(const std::string& name, Args&&... args)
{
   T* obj = new T(std::forward<Args>(args)...);
   REQUIRE(obj->registerObject(name.c_str()));
   return obj;
}

template <class T, class... Args>
static T* createRegisteredObjectWithId(const std::string& name, U32 id, Args&&... args)
{
   T* obj = new T(std::forward<Args>(args)...);
   REQUIRE(obj->registerObject(name.c_str(), id));
   return obj;
}

TEST_CASE("SimObject registration and identity", "[SimObject]") {
   const std::string rootName = uniqueName("simObject");
   const U32 customId = 100001 + gNextTestObjectId.fetch_add(1);

   TestSimObject* obj = createRegisteredObjectWithId<TestSimObject>(rootName, customId);

   REQUIRE(obj->isProperlyAdded());
   REQUIRE(obj->getName() == StringTable->insert(rootName.c_str()));
   REQUIRE(obj->getId() == customId);
   REQUIRE(Sim::findObject(rootName.c_str()) == obj);
   REQUIRE(Sim::findObject(customId) == obj);
   REQUIRE(obj->getGroup() == nullptr);
   REQUIRE_FALSE(obj->isChildOfGroup(Sim::getRootGroup()));
   REQUIRE(obj->mOnAddCount == 1);
   REQUIRE(obj->mOnNameChangeCount == 1);

   TestSimObject* idOnly = new TestSimObject();
   REQUIRE(idOnly->registerObject(customId + 2000));
   REQUIRE(idOnly->getId() == customId + 2000);
   REQUIRE(idOnly->getName() == nullptr);
   idOnly->deleteObject();

   const std::string renamed = uniqueName("renamed");
   obj->assignName(renamed.c_str());
   REQUIRE(obj->mOnNameChangeCount == 2);
   REQUIRE(obj->mLastName == renamed);
   REQUIRE(Sim::findObject(rootName.c_str()) == nullptr);
   REQUIRE(Sim::findObject(renamed.c_str()) == obj);

   const U32 newId = customId + 1000;
   obj->setId(newId);
   REQUIRE(obj->getId() == newId);
   REQUIRE(Sim::findObject(newId) == obj);
   REQUIRE(Sim::findObject(customId) == nullptr);

   obj->setProgenitorFile("test/simBaseTests.cc");
   REQUIRE(std::string(obj->getProgenitorFile()) == "test/simBaseTests.cc");

   obj->setPeriodicTimerID(42);
   REQUIRE(obj->isPeriodicTimerActive());
   REQUIRE(obj->getPeriodicTimerID() == 42);
   obj->setPeriodicTimerID(0);
   REQUIRE_FALSE(obj->isPeriodicTimerActive());

   obj->deleteObject();
   REQUIRE(Sim::findObject(renamed.c_str()) == nullptr);
}

TEST_CASE("SimObject fields and copy helpers", "[SimObject]") {
   TestSimObject* source = createRegisteredObject<TestSimObject>(uniqueName("source"));
   TestSimObject* target = createRegisteredObject<TestSimObject>(uniqueName("target"));

   REQUIRE(source->processArguments(0, nullptr));
   const char* argList[] = {"alpha"};
   REQUIRE_FALSE(source->processArguments(1, argList));
   REQUIRE(source->mLastProcessArgc == 1);
   REQUIRE(source->mLastProcessArg0 == "alpha");

   REQUIRE(source->getDataFieldType(StringTable->insert("testValue"), nullptr) == TypeS32);
   source->mTestValue = 37;
   char staticBuffer[1024] = {};
   MemStream staticStream(sizeof(staticBuffer), staticBuffer, true, true);
   source->writeFields(staticStream, 0);
   REQUIRE(std::string(staticBuffer).find("testValue") != std::string::npos);
   REQUIRE(std::string(staticBuffer).find("37") != std::string::npos);

   source->setDataFieldDynamic(StringTable->insert("dynamicField"), nullptr, "dynamicValue", TypeString);
   REQUIRE(std::string(source->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)) == "dynamicValue");

   source->setDataFieldDynamic(StringTable->insert("typedField"), nullptr, "123", TypeS32);
   U32 typedFieldType = 0;
   REQUIRE(std::string(source->getDataFieldDynamic(StringTable->insert("typedField"), nullptr, &typedFieldType)) == "123");
   REQUIRE(typedFieldType == TypeS32);
   source->setInternalName("sourceInternal");
   REQUIRE(source->getInternalName() == StringTable->insert("sourceInternal"));

   TestSimObject* copied = createRegisteredObject<TestSimObject>(uniqueName("copied"));
   source->setClassNamespace("sourceClass");
   source->setSuperClassNamespace("sourceSuper");
   source->copyTo(copied);
   REQUIRE(copied->getClassNamespace() == StringTable->insert("sourceClass"));
   REQUIRE(copied->getSuperClassNamespace() == StringTable->insert("sourceSuper"));

   target->setInternalName("targetInternal");
   REQUIRE_NOTHROW(target->assignFieldsFrom(source));

   target->assignDynamicFieldsFrom(source);
   REQUIRE(std::string(target->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)) == "dynamicValue");
   REQUIRE(std::string(target->getDataFieldDynamic(StringTable->insert("typedField"), nullptr)) == "123");

   TestSimObject* cloneNoDyn = dynamic_cast<TestSimObject*>(source->clone(false));
   REQUIRE(cloneNoDyn != nullptr);
   REQUIRE(std::string(cloneNoDyn->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)).empty());
   cloneNoDyn->deleteObject();

   TestSimObject* cloneWithDyn = dynamic_cast<TestSimObject*>(source->clone(true));
   REQUIRE(cloneWithDyn != nullptr);
   REQUIRE(std::string(cloneWithDyn->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)) == "dynamicValue");
   REQUIRE(std::string(cloneWithDyn->getDataFieldDynamic(StringTable->insert("typedField"), nullptr)) == "123");
   cloneWithDyn->deleteObject();

   source->clearDynamicFields();
   REQUIRE(std::string(source->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)).empty());
   REQUIRE(std::string(source->getDataFieldDynamic(StringTable->insert("typedField"), nullptr)).empty());

   target->deleteObject();
   copied->deleteObject();
   source->deleteObject();
}

TEST_CASE("SimObject set membership helpers", "[SimObject][SimSet]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("set"));
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("member"));

   REQUIRE_FALSE(obj->addToSet("does_not_exist"));
   REQUIRE(obj->addToSet(set->getName()));
   REQUIRE(set->size() == 1);
   REQUIRE(set->front() == obj);
   REQUIRE(set->last() == obj);
   REQUIRE(obj->getGroup() == nullptr);

   REQUIRE(obj->removeFromSet(set->getId()));
   REQUIRE(set->size() == 0);

   REQUIRE(obj->addToSet(set->getId()));
   REQUIRE(set->size() == 1);
   REQUIRE(obj->removeFromSet(set->getName()));
   REQUIRE(set->size() == 0);

   obj->deleteObject();
   set->deleteObject();
}

TEST_CASE("SimSet container operations and traversal", "[SimSet]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("set"));
   TestSimObject* first = createRegisteredObject<TestSimObject>(uniqueName("first"));
   TestSimObject* middle = createRegisteredObject<TestSimObject>(uniqueName("middle"));
   TestSimObject* last = createRegisteredObject<TestSimObject>(uniqueName("last"));
   TestSimObject* child = createRegisteredObject<TestSimObject>(uniqueName("child"));
   SimSet* nested = createRegisteredObject<SimSet>(uniqueName("nested"));

   REQUIRE(set->empty());
   set->addObject(first);
   set->addObject(middle);
   set->addObject(last);
   set->addObject(nested);
   nested->addObject(child);

   REQUIRE(set->size() == 4);
   REQUIRE(set->front() == first);
   REQUIRE(set->first() == first);
   REQUIRE(set->last() == nested);
   REQUIRE(set->operator[](0) == first);
   REQUIRE(set->at(1) == middle);
   REQUIRE(set->containsType<SimSet>());
   REQUIRE(set->containsType<TestSimObject>());

   std::vector<SimObject*> iterated;
   for (SimSetIterator itr(set); *itr; ++itr)
      iterated.push_back(*itr);

   REQUIRE(iterated.size() == 5);
   REQUIRE(iterated[0] == first);
   REQUIRE(iterated[1] == middle);
   REQUIRE(iterated[2] == last);
   REQUIRE(iterated[3] == nested);
   REQUIRE(iterated[4] == child);

   REQUIRE(set->reOrder(last, middle));
   REQUIRE(set->operator[](0) == first);
   REQUIRE(set->operator[](1) == last);
   REQUIRE(set->operator[](2) == middle);
   REQUIRE(set->operator[](3) == nested);

   set->pushObjectToBack(first);
   REQUIRE(set->last() == first);
   set->bringObjectToFront(first);
   REQUIRE(set->first() == first);

   set->popObject();
   REQUIRE(set->size() == 3);
   REQUIRE(set->last() == middle);

   set->pushObject(nested);
   REQUIRE(set->size() == 4);
   REQUIRE(set->last() == nested);

   middle->setInternalName("middleInternal");
   child->setInternalName("childInternal");
   REQUIRE(set->findObjectByInternalName(StringTable->insert("middleInternal"), false) == middle);
   REQUIRE(set->findObjectByInternalName(StringTable->insert("childInternal"), false) == nullptr);
   REQUIRE(set->findObjectByInternalName(StringTable->insert("childInternal"), true) == child);

   REQUIRE(set->findObject(first->getName()) == first);
   const std::string nestedPath = std::string(nested->getName()) + "/" + child->getName();
   REQUIRE(set->findObject(nestedPath.c_str()) == child);

   REQUIRE(first->mConsoleCallCount == 0);
   REQUIRE(child->mConsoleCallCount == 0);

   set->callOnChildren("bumpCounter", 0, nullptr, false);
   REQUIRE(first->mConsoleCallCount == 1);
   REQUIRE(middle->mConsoleCallCount == 1);
   REQUIRE(last->mConsoleCallCount == 1);
   REQUIRE(child->mConsoleCallCount == 0);

   set->callOnChildren("bumpCounter", 0, nullptr, true);
   REQUIRE(first->mConsoleCallCount == 2);
   REQUIRE(middle->mConsoleCallCount == 2);
   REQUIRE(last->mConsoleCallCount == 2);
   REQUIRE(child->mConsoleCallCount == 1);

   char buffer[4096] = {};
   MemStream stream(sizeof(buffer), buffer, true, true);
   set->write(stream, 0);
   REQUIRE(std::string(buffer).find("new SimSet(") != std::string::npos);
   REQUIRE(std::string(buffer).find(first->getName()) != std::string::npos);

   nested->deleteObject();
   first->deleteObject();
   middle->deleteObject();
   last->deleteObject();
   set->deleteObject();
}

TEST_CASE("SimSet deletion and clear behavior", "[SimSet]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("deleteSet"));
   TestSimObject* a = createRegisteredObject<TestSimObject>(uniqueName("a"));
   TestSimObject* b = createRegisteredObject<TestSimObject>(uniqueName("b"));

   set->addObject(a);
   set->addObject(b);
   SimObject* trackedA = a;
   SimObject* trackedB = b;
   a->registerReference(&trackedA);
   b->registerReference(&trackedB);

   set->clear();
   REQUIRE(set->size() == 0);
   REQUIRE(trackedA == a);
   REQUIRE(trackedB == b);
   REQUIRE(Sim::findObject(a->getId()) == a);
   REQUIRE(Sim::findObject(b->getId()) == b);

   set->addObject(a);
   set->addObject(b);
   set->deleteObjects();
   REQUIRE(trackedA == nullptr);
   REQUIRE(trackedB == nullptr);
   REQUIRE(set->size() == 0);

   set->deleteObject();
}

TEST_CASE("SimGroup hierarchy and recursive lookup", "[SimGroup]") {
   SimGroup* outer = createRegisteredObject<SimGroup>(uniqueName("outer"));
   SimGroup* inner = createRegisteredObject<SimGroup>(uniqueName("inner"));
   TestSimObject* child = createRegisteredObject<TestSimObject>(uniqueName("child"));
   TestSimObject* mover = createRegisteredObject<TestSimObject>(uniqueName("mover"));

   REQUIRE(outer->processArguments(0, nullptr));

   outer->addObject(inner);
   inner->addObject(child);
   REQUIRE(child->getGroup() == inner);
   REQUIRE(child->isChildOfGroup(inner));
   REQUIRE(child->isChildOfGroup(outer));
   REQUIRE(child->mOnGroupAddCount == 1);

   const std::string childPath = std::string(inner->getName()) + "/" + child->getName();
   REQUIRE(outer->findObject(childPath.c_str()) == child);
   REQUIRE(inner->findObject(child->getName()) == child);

   outer->addObject(mover);
   REQUIRE(mover->getGroup() == outer);
   inner->addObject(mover);
   REQUIRE(mover->getGroup() == inner);
   REQUIRE(mover->mOnGroupAddCount == 2);
   const std::string moverPath = std::string(inner->getName()) + "/" + mover->getName();
   REQUIRE(outer->findObject(moverPath.c_str()) == mover);

   const std::string directName = uniqueName("namedChild");
   TestSimObject* named = createRegisteredObject<TestSimObject>(directName);
   inner->addObject(named, 424242);
   REQUIRE(named->getId() == 424242);
   REQUIRE(inner->findObject(named->getName()) == named);
   REQUIRE(named->mOnGroupAddCount == 1);

   TestSimObject* renamed = createRegisteredObject<TestSimObject>(uniqueName("renameChild"));
   inner->addObject(renamed, "assignedName");
   REQUIRE(std::string(renamed->getName()) == "assignedName");
   REQUIRE(inner->findObject("assignedName") == renamed);
   REQUIRE(renamed->mOnGroupAddCount == 1);

   std::vector<SimObject*> groupIterated;
   for (SimGroupIterator itr(outer); *itr; ++itr)
      groupIterated.push_back(*itr);
   REQUIRE(groupIterated.size() == 5);
   REQUIRE(groupIterated[0] == inner);
   REQUIRE(groupIterated[1] == child);
   REQUIRE(groupIterated[2] == mover);
   REQUIRE(groupIterated[3] == named);
   REQUIRE(groupIterated[4] == renamed);

   inner->removeObject(renamed);
   REQUIRE(renamed->getGroup() == nullptr);
   REQUIRE(renamed->mOnGroupRemoveCount == 1);

   SimObject* trackedChild = child;
   SimObject* trackedInner = inner;
   child->registerReference(&trackedChild);
   inner->registerReference(&trackedInner);
   outer->deleteObject();
   REQUIRE(trackedChild == nullptr);
   REQUIRE(trackedInner == nullptr);
   renamed->deleteObject();
}
