/*🍲Ketl🍲*/
#ifndef gc_allocator_h
#define gc_allocator_h

#include <map>
#include <set>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>

namespace Ketl {

	template <typename Allocator>
	class GCAllocator {
	public:

		GCAllocator(Allocator& alloc)
			: _alloc(alloc) {}
		~GCAllocator() {
			_refRoots.clear();
			collectGarbage();
		}

		using Finalizer = void(*)(void*);

		static void emptyFinalizer(void*) {};

		class RefHolder {
		public:

			inline RefHolder& registerAbsLink(const void* ptr) {
				_absLinks.emplace_back(ptr);
				return *this;
			}
			template <typename T>
			inline RefHolder& registerRefLink(const T* const* pptr) {
				_refLinks.emplace_back(reinterpret_cast<const void* const*>(pptr));
				return *this;
			}

			inline const auto& absLinks() const {
				return _absLinks;
			}
			inline const auto& refLinks() const {
				return _refLinks;
			}

		private:
			std::vector<const void*> _absLinks;
			std::vector<const void* const*> _refLinks;
		};

		RefHolder& registerRefMemory(void* ptr, size_t size) {
			auto it = _objects.try_emplace(Object(_currUsageFlag, ptr, size, nullptr)).first;
			return it->second;
		}

		RefHolder& registerMemory(void* ptr, size_t size, std::nullptr_t) = delete;
		RefHolder& registerMemory(void* ptr, size_t size, Finalizer finalizer = &emptyFinalizer) {
			auto it = _objects.try_emplace(Object(_currUsageFlag, ptr, size, finalizer)).first;
			_managedObject.emplace_back(it);
			return it->second;
		}

		template <typename T>
		inline void registerAbsRoot(const T* ptr) {
			++_absRoots[reinterpret_cast<const void*>(ptr)];
		}

		template <typename T>
		inline void unregisterAbsRoot(const T* ptr) {
			auto it = _absRoots.find(reinterpret_cast<const void*>(ptr));
			if (it == _absRoots.end()) {
				// TODO error!
				__debugbreak();
				return;
			}
			--it->second;
			if (it->second == 0) {
				_absRoots.erase(it);
			}
		}

		template <typename T>
		inline void registerRefRoot(const T* const* pptr) {
			++_refRoots[reinterpret_cast<const void* const*>(pptr)];
		}
		template <typename T>
		inline void unregisterRefRoot(const T* const* pptr) {
			auto it = _refRoots.find(reinterpret_cast<const void* const*>(pptr));
			if (it == _refRoots.end()) {
				// TODO error!
				__debugbreak();
				return;
			}
			--it->second;
			if (it->second == 0) {
				_refRoots.erase(it);
			}
		}

		template <typename T1, typename T2>
		inline void registerAbsLink(const T1* t1ptr, const T2* t2ptr) {
			auto refHolder = findRefHolder(reinterpret_cast<const void*>(t1ptr));
			if (refHolder == nullptr) {
				return;
			}
			refHolder->registerAbsLink(t2ptr);
		}
		template <typename T1, typename T2>
		inline void registerRefLink(const T1* t1ptr, const T2* const* t2pptr) {
			auto refHolder = findRefHolder(reinterpret_cast<const void*>(t1ptr));
			if (refHolder == nullptr) {
				return;
			}
			refHolder->registerRefLink(t2pptr);
		}

		inline size_t collectGarbage() {
			if (_objects.empty()) {
				return 0u;
			}

			_currUsageFlag = !_currUsageFlag;
			markStage();
			return swipeStage();
		}

		inline void clearRoots() {
			_refRoots.clear();
		}

	private:

		RefHolder* findRefHolder(const void* ptr) {
			auto it = _objects.find(Object(ptr));
			if (it == _objects.end()) {
				return nullptr;
			}

			return &it->second;
		}

		void markStage();
		size_t swipeStage();

		void fillLinks(const std::set<const void*>& source, std::set<const void*>& target) const;

		class Object {
		public:

			// for search purposes only
			Object(const void* ptr)
				: _usageFlag(false), _ptr(const_cast<void*>(ptr)), _size(0), _finalizer(&emptyFinalizer) {}
			Object(bool usageFlag, void* ptr, size_t size, Finalizer finalizer)
				: _usageFlag(usageFlag), _ptr(ptr), _size(size), _finalizer(finalizer) {}
			Object(const Object& other) = delete;
			Object(Object&& other) noexcept
				: _usageFlag(other._usageFlag), _ptr(other._ptr), _size(other._size), _finalizer(other._finalizer) {
				other._finalizer = &emptyFinalizer;
			}

			inline void finalize() const {
				_finalizer(_ptr);
			}

			inline friend bool operator<(const Object& lhs, const Object& rhs) {
				return lhs._ptr < rhs._ptr;
			}

			inline bool getUsageFlag() const {
				return _usageFlag;
			}
			inline void updateUsageFlag(bool usageFlag) const {
				_usageFlag = usageFlag;
			}

			inline size_t size() const { return _size; }
			inline void* begin() const { return _ptr; }
			inline void* end() const { return reinterpret_cast<uint8_t*>(_ptr) + _size; }

		private:
			mutable bool _usageFlag;
			void* _ptr;
			size_t _size;
			Finalizer _finalizer;
		};

		bool _currUsageFlag = false;
		Allocator& _alloc;

		std::unordered_map<const void*, uint64_t> _absRoots;
		std::unordered_map<const void* const*, uint64_t> _refRoots;
		std::map<Object, RefHolder> _objects;
		std::list<typename decltype(_objects)::iterator> _managedObject;
	};

	template <typename Allocator>
	void GCAllocator<Allocator>::fillLinks(const std::set<const void*>& source, std::set<const void*>& target) const {
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

	template <typename Allocator>
	void GCAllocator<Allocator>::markStage() {
		bool linkChoise = true;
		std::set<const void*> links1;
		std::set<const void*> links2;

		auto* linksSourcePtr = &links1;
		auto* linksTargetPtr = &links2;

		for (const auto& abs : _absRoots) {
			linksSourcePtr->emplace(abs.first);
		}
		for (const auto& ref : _refRoots) {
			linksSourcePtr->emplace(*ref.first);
		}

		while (!linksSourcePtr->empty()) {
			fillLinks(*linksSourcePtr, *linksTargetPtr);

			linksSourcePtr = &(linkChoise ? links2 : links1);
			linksTargetPtr = &(linkChoise ? links1 : links2);

			linkChoise = !linkChoise;
			linksTargetPtr->clear();
		}
	}

	template <typename Allocator>
	size_t GCAllocator<Allocator>::swipeStage() {
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

#endif /*gc_allocator_h*/