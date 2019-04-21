#ifndef CONSENSUS_VERSIONBITS_H
#define CONSENSUS_VERSIONBITS_H

//#include "chain.h"
#include <map>

/** What block version to use for new blocks (pre versionbits) */
static const int32_t VERSIONBITS_LAST_OLD_BLOCK_VERSION = 4;
/** What bits to set in version for versionbits blocks */
static const int32_t VERSIONBITS_TOP_BITS = 0x20000000UL;
/** What bitmask determines whether versionbits is in use */
static const int32_t VERSIONBITS_TOP_MASK = 0xE0000000UL;
/** Total bits available for versionbits */
static const int32_t VERSIONBITS_NUM_BITS = 29;

enum ThresholdState {
    THRESHOLD_DEFINED,
    THRESHOLD_STARTED,
    THRESHOLD_LOCKED_IN,
    THRESHOLD_ACTIVE,
    THRESHOLD_FAILED,
};

// A map that gives the state for blocks whose height is a multiple of Period().
// The map is indexed by the block's parent, however, so all keys in the map
// will either be NULL or a block with (height + 1) % Period() == 0.
typedef std::map<const CBlockIndex*, ThresholdState> ThresholdConditionCache;

struct BIP9DeploymentInfo {
    /** Deployment name */
    const char *name;
    /** Whether GBT clients can safely ignore this rule in simplified usage */
    bool gbt_force;
};

extern const struct BIP9DeploymentInfo VersionBitsDeploymentInfo[];

/**
 * Abstract class that implements BIP9-style threshold logic, and caches results.
 */
class AbstractThresholdConditionChecker {
protected:
    virtual bool Condition(const CBlockIndex* pindex, CIface *params) const =0;
    virtual int64_t BeginTime(CIface *params) const =0;
    virtual int64_t EndTime(CIface *params) const =0;
    virtual int Period(CIface *params) const =0;
    virtual int Threshold(CIface *params) const =0;

public:
    // Note that the function below takes a pindexPrev as input: they compute information for block B based on its parent.
    ThresholdState GetStateFor(const CBlockIndex* pindexPrev, CIface *params, ThresholdConditionCache& cache) const;
};

struct VersionBitsCache
{
    ThresholdConditionCache caches[MAX_VERSION_BITS_DEPLOYMENTS];

    void Clear();
};

ThresholdState VersionBitsState(const CBlockIndex* pindexPrev, CIface *params, DeploymentPos pos, VersionBitsCache& cache);
uint32_t VersionBitsMask(CIface *params, DeploymentPos pos);

#endif

