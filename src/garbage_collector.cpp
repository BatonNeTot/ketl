/*🍲Ketl🍲*/
#include "garbage_collector.h"

#include "context.h"


namespace Ketl {

	void GarbageCollector::fillLinks(const std::set<const void*>& source, std::set<const void*>& target) const {
		auto itLinks = source.cbegin();
		auto endLinks = source.cend();
		auto itHeap = _objects.begin();
		auto endHeap = _objects.end();

		for (;;) {
			if (itLinks == endLinks) {
				return;
			}
			if (*itLinks < itHeap->first.begin()) {
				++itLinks;
				continue;
			}

			if (*itLinks >= itHeap->first.end()) {
				++itHeap;
				if (itHeap == endHeap) {
					return;
				}
				continue;
			}

			const auto& obj = itHeap->first;
			if (obj.getUsageFlag() != _currUsageFlag) {
				obj.updateUsageFlag(_currUsageFlag);

				auto& refs = itHeap->second;
				for (const auto& abs : refs.absLinks()) {
					target.emplace(abs);
				}
				for (const auto& ref : refs.refLinks()) {
					target.emplace(*ref);
				}
			}

			++itHeap;
			if (itHeap == endHeap) {
				return;
			}
		}
	}

	void GarbageCollector::markStage() {
		bool linkChoise = true;
		std::set<const void*> links1;
		std::set<const void*> links2;

		auto* linksSourcePtr = &links1;
		auto* linksTargetPtr = &links2;

		for (const auto& abs : _absRoots) {
			linksSourcePtr->emplace(abs);
		}
		for (const auto& ref : _refRoots) {
			linksSourcePtr->emplace(*ref);
		}

		while (!linksSourcePtr->empty()) {
			fillLinks(*linksSourcePtr, *linksTargetPtr);

			linksSourcePtr = &(linkChoise ? links2 : links1);
			linksTargetPtr = &(linkChoise ? links1 : links2);

			linkChoise = !linkChoise;
			linksTargetPtr->clear();
		}
	}

	size_t GarbageCollector::swipeStage() {
		size_t totalCleared = 0u;

		for (auto mIt = _managedObject.begin(), end = _managedObject.end(); mIt != end;) {
			auto& objectIt = *mIt;
			if (objectIt->first.getUsageFlag() != _currUsageFlag) {
				totalCleared += objectIt->first.size();

				objectIt->first.finalize();
				auto ptr = objectIt->first.begin();
				_alloc.deallocate(ptr);

				_objects.erase(objectIt);
				mIt = _managedObject.erase(mIt);
			}
			else {
				++mIt;
			}
		}

		return totalCleared;
	}
}