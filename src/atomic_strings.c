//🍲ketl
#include "ketl/atomic_strings.h"

#include <stdlib.h>
#include <string.h>

// TODO make m's and p's bigger
const static uint32_t m1 = 1000000007;
const static uint32_t m2 = 1000000009;

#define MAX_STRING_LENGTH 100

const static uint64_t powP1[MAX_STRING_LENGTH] = {
	1ULL, 31ULL, 961ULL, 29791ULL, 923521ULL, 28629151ULL, 
	887503681ULL, 512613922ULL, 891031477ULL, 621975598ULL, 
	281243405ULL, 718545499ULL, 274910315ULL, 522219709ULL, 
	188810867ULL, 853136842ULL, 447241920ULL, 864499429ULL, 
	799482117ULL, 783945459ULL, 302309061ULL, 371580828ULL, 
	519005591ULL, 89173209ULL, 764369465ULL, 695453254ULL, 
	559050727ULL, 330572418ULL, 247744888ULL, 680091479ULL, 
	82835702ULL, 567906748ULL, 605109069ULL, 758381013ULL, 
	509811242ULL, 804148397ULL, 928600139ULL, 786604113ULL, 
	384727335ULL, 926547308ULL, 722966352ULL, 411956758ULL, 
	770659414ULL, 890441673ULL, 603691674ULL, 714441768ULL, 
	147694654ULL, 578534246ULL, 934561507ULL, 971406521ULL, 
	113601941ULL, 521660150ULL, 171464538ULL, 315400643ULL, 
	777419870ULL, 100015802ULL, 100489841ULL, 115185050ULL, 
	570736529ULL, 692832280ULL, 477800533ULL, 811816425ULL, 
	166309000ULL, 155578965ULL, 822947887ULL, 511384322ULL, 
	852913877ULL, 440330005ULL, 650230064ULL, 157131844ULL, 
	871087136ULL, 3701027ULL, 114731837ULL, 556686926ULL, 
	257294587ULL, 976132148ULL, 260096378ULL, 62987662ULL, 
	952617515ULL, 531142762ULL, 465425510ULL, 428190712ULL, 
	273911981ULL, 491271355ULL, 229411900ULL, 111768851ULL, 
	464834360ULL, 409865062ULL, 705816838ULL, 880321831ULL, 
	289976572ULL, 989273676ULL, 667483746ULL, 691995986ULL, 
	451875419ULL, 8137891ULL, 252274621ULL, 820513202ULL, 
	435909087ULL, 513181606ULL
};
const static uint64_t powP2[MAX_STRING_LENGTH] = {
	1ULL, 37ULL, 1369ULL, 50653ULL, 1874161ULL, 69343957ULL, 
	565726391ULL, 931876287ULL, 479422313ULL, 738625428ULL, 
	329140593ULL, 178201833ULL, 593467767ULL, 958307190ULL, 
	457365715ULL, 922531311ULL, 133658201ULL, 945353401ULL, 
	978075531ULL, 188794323ULL, 985389897ULL, 459425865ULL, 
	998756861ULL, 954003533ULL, 298130406ULL, 30824923ULL, 
	140522142ULL, 199319209ULL, 374810670ULL, 867994673ULL, 
	115802613ULL, 284696645ULL, 533775775ULL, 749703504ULL, 
	739029405ULL, 344087742ULL, 731246346ULL, 56114559ULL, 
	76238665ULL, 820830587ULL, 370731449ULL, 717063496ULL, 
	531349118ULL, 659917195ULL, 416935999ULL, 426631828ULL, 
	785377501ULL, 58967276ULL, 181789194ULL, 726200124ULL, 
	869404354ULL, 167960810ULL, 214549916ULL, 938346829ULL, 
	718832367ULL, 596797345ULL, 81501567ULL, 15557952ULL, 
	575644224ULL, 298836099ULL, 56935564ULL, 106615850ULL, 
	944786423ULL, 957097345ULL, 412601450ULL, 266253515ULL, 
	851379974ULL, 501058759ULL, 539173921ULL, 949434906ULL, 
	129091207ULL, 776374623ULL, 725860799ULL, 856849329ULL, 
	703424894ULL, 26720844ULL, 988671228ULL, 580835112ULL, 
	490898955ULL, 163261173ULL, 40663347ULL, 504543830ULL, 
	668121548ULL, 720497060ULL, 658390986ULL, 360466266ULL, 
	337251725ULL, 478313717ULL, 697607376ULL, 811472687ULL, 
	24489149ULL, 906098513ULL, 525644684ULL, 448853137ULL, 
	607565925ULL, 479939027ULL, 757743846ULL, 36522050ULL, 
	351315841ULL, 998686009ULL
};

uint64_t ketlHashString(const char* str, uint64_t length) {
	if (length != KETL_NULL_TERMINATED_LENGTH && length > MAX_STRING_LENGTH) {
		return 0;
	}

	uint32_t firstPart = 0;
	uint32_t secondPart = 0;

	uint64_t counter = 0;

	KETL_FOREVER {
		char symbol = str[counter];
		if (symbol == '\0' || counter >= length) {
			return (((uint64_t)firstPart) << 32) + secondPart;
		}

		if (counter >= MAX_STRING_LENGTH) {
			return 0;
		}

		firstPart = (firstPart + (symbol + 1 - 'a') * powP1[counter]) % m1;
		secondPart = (secondPart + (symbol + 1 - 'a') * powP2[counter]) % m2;

		++counter;
	}
}

static const uint64_t primeCapacities[] =
{
  3ULL, 11UL, 23ULL, 53ULL, 97ULL, 193ULL, 389ULL,
  769ULL, 1543ULL, 3079ULL, 6151ULL, 12289ULL, 
  24593ULL, 49157ULL, 98317ULL, 196613ULL, 393241ULL,
  786433ULL, 1572869ULL, 3145739ULL, 6291469ULL, 
  12582917ULL, 25165843ULL, 50331653ULL, 100663319ULL,
  201326611ULL, 402653189ULL, 805306457ULL, 
  1610612741ULL, 3221225473ULL, 4294967291ULL
};

static const uint64_t TOTAL_PRIME_CAPACITIES = sizeof(primeCapacities) / sizeof(primeCapacities[0]);

struct KETLAtomicStringsBucketBase {
	const char* key;
	uint64_t hash;
	KETLAtomicStringsBucketBase* next;
};

void ketlInitAtomicStrings(KETLAtomicStrings* strings, size_t poolSize) {
	// TODO align
	ketlInitObjectPool(&strings->stringPool, sizeof(char), 256);
	ketlInitObjectPool(&strings->bucketPool, sizeof(KETLAtomicStringsBucketBase), poolSize);
	strings->capacityIndex = 0;
	uint64_t capacity = primeCapacities[0];
	strings->size = 0;
	uint64_t arraySize = sizeof(KETLAtomicStringsBucketBase*) * capacity;
	KETLAtomicStringsBucketBase** buckets = strings->buckets = malloc(arraySize);
	// TODO use custom memset
	memset(buckets, 0, arraySize);
}

void ketlDeinitAtomicStrings(KETLAtomicStrings* strings) {
	free(strings->buckets);
	ketlDeinitObjectPool(&strings->bucketPool);
	ketlDeinitObjectPool(&strings->stringPool);
}

static bool isStrEqual(const char* bucketKey, const char* key, uint64_t length) {
	uint64_t counter = 0;
	KETL_FOREVER {
		char lhs = bucketKey[counter];
		char rhs = key[counter];

		bool endLhs = lhs == '\0';
		bool endRhs = rhs == '\0' || length <= counter;

		if (endLhs || endRhs) {
			return endLhs == endRhs;
		}

		if (lhs != rhs) {
			return false;
		}

		++counter;
	}
}

static const char* const emptyString = "";

const char* ketlAtomicStringsGet(KETLAtomicStrings* map, const char* key, uint64_t length) {
	if (length == 0 || *key == '\0') {
		return emptyString;
	}

	uint64_t capacity = primeCapacities[map->capacityIndex];
	KETLAtomicStringsBucketBase** buckets = map->buckets;
	uint64_t hash = ketlHashString(key, length);
	if (hash == 0) {
		return NULL;
	}

	uint64_t index = hash % capacity;
	KETLAtomicStringsBucketBase* bucket = buckets[index];

	while (bucket) {
		if (bucket->hash == hash && isStrEqual(bucket->key, key, length)) {
			return bucket->key;
		}

		bucket = bucket->next;
	}

	uint64_t size = ++map->size;
	if (size > capacity) {
		uint64_t newCapacity = primeCapacities[++map->capacityIndex];
		uint64_t arraySize = sizeof(KETLAtomicStringsBucketBase*) * newCapacity;
		KETLAtomicStringsBucketBase** newBuckets = map->buckets = malloc(arraySize);
		// TODO use custom memset
		memset(newBuckets, 0, arraySize);
		for (uint64_t i = 0; i < capacity; ++i) {
			bucket = buckets[i];
			while (bucket) {
				KETLAtomicStringsBucketBase* next = bucket->next;
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

	bucket = ketlGetFreeObjectFromPool(&map->bucketPool);

	if (length == KETL_NULL_TERMINATED_LENGTH) {
		length = strlen(key);
	}

	char* bucketKey = ketlGetNFreeObjectsFromPool(&map->stringPool, length + 1);

	memcpy(bucketKey, key, length);
	bucketKey[length] = '\0';

	bucket->key = bucketKey;
	bucket->hash = hash;
	bucket->next = buckets[index];

	buckets[index] = bucket;
	return bucketKey;
}