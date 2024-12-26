//🫖ketl

KETL_DEFINE(ketl_atomic_strings_bucket) {
	const char* key;
	uint64_t hash;
	ketl_atomic_strings_bucket* next;
};

void ketl_atomic_strings_init(ketl_atomic_strings* strings, size_t poolSize) {
	// TODO align
	ketl_object_pool_init(&strings->stringPool, sizeof(char), 256);
	ketl_object_pool_init(&strings->bucketPool, sizeof(ketl_atomic_strings_bucket), poolSize);
	strings->capacityIndex = 0;
	uint64_t capacity = ketl_prime_capacities[0];
	strings->size = 0;
	uint64_t arraySize = sizeof(ketl_atomic_strings_bucket*) * capacity;
	ketl_atomic_strings_bucket** buckets = strings->buckets = malloc(arraySize);
	// TODO use custom memset
	memset(buckets, 0, arraySize);
}

void ketl_atomic_strings_deinit(ketl_atomic_strings* strings) {
	free(strings->buckets);
	ketl_object_pool_deinit(&strings->bucketPool);
	ketl_object_pool_deinit(&strings->stringPool);
}

const char* ketl_atomic_strings_get(ketl_atomic_strings* map, const char* key, uint64_t length) {
	if (key == NULL) {
		return NULL;
	}
	if (length == 0 || *key == '\0') {
		return emptyString;
	}

	uint64_t capacity = ketl_prime_capacities[map->capacityIndex];
	ketl_atomic_strings_bucket** buckets = map->buckets;
	uint64_t hash = ketl_hash_string(key, length);
	if (hash == 0) {
		return NULL;
	}

	uint64_t index = hash % capacity;
	ketl_atomic_strings_bucket* bucket = buckets[index];

	while (bucket) {
		if (bucket->hash == hash && isStrEqual(bucket->key, key, length)) {
			return bucket->key;
		}

		bucket = bucket->next;
	}

	uint64_t size = ++map->size;
	if (size > capacity) {
		uint64_t newCapacityIndex = map->capacityIndex + 1;
		if (KETL_PRIME_CAPACITIES_TOTAL <= newCapacityIndex) {
			// TODO error
			return NULL;
		}
		uint64_t newCapacity = ketl_prime_capacities[map->capacityIndex = newCapacityIndex];
		uint64_t arraySize = sizeof(ketl_atomic_strings_bucket*) * newCapacity;
		ketl_atomic_strings_bucket** newBuckets = map->buckets = malloc(arraySize);
		// TODO use custom memset
		memset(newBuckets, 0, arraySize);
		for (uint64_t i = 0; i < capacity; ++i) {
			bucket = buckets[i];
			while (bucket) {
				ketl_atomic_strings_bucket* next = bucket->next;
				uint64_t newIndex = bucket->hash % newCapacity;
				bucket->next = newBuckets[newIndex];
				newBuckets[newIndex] = bucket;
				bucket = next;
			}
		}
		free(buckets);
		index = hash % newCapacity;
		buckets = newBuckets;
	}

	bucket = ketl_object_pool_get(&map->bucketPool);

	if (length == KETL_NULL_TERMINATED_LENGTH) {
		length = strlen(key);
	}

	char* bucketKey = ketl_object_pool_get_array(&map->stringPool, length + 1);

	memcpy(bucketKey, key, length);
	bucketKey[length] = '\0';

	bucket->key = bucketKey;
	bucket->hash = hash;
	bucket->next = buckets[index];

	buckets[index] = bucket;
	return bucketKey;
}