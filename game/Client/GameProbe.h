#pragma once
#ifndef DXA_GAME_PROBE_TIMEOUT_START
#define DXA_GAME_PROBE_TIMEOUT_START 0
#endif
// Included only in the explicitly selected diagnostic build.
#include "ReferenceFrameCapture.hpp"
#include "ArenaAppearance.h"
#if defined(DXA_GAME_PROBE_BIANCA)
#include "ReferenceBiancaSkillProbe.hpp"
#define DXA_GAME_PROBE_FUNCTION ReferenceBiancaSkillProbe
#elif defined(DXA_GAME_PROBE_NICKY)
#include "ReferenceNickySkillProbe.hpp"
#define DXA_GAME_PROBE_FUNCTION ReferenceNickySkillProbe
#elif defined(DXA_GAME_PROBE_LOOT)
#include "ReferenceLootCraftProbe.hpp"
#define DXA_GAME_PROBE_FUNCTION ReferenceLootCraftProbe
#elif defined(DXA_GAME_PROBE_COMBAT)
#include "ReferenceCombatProbe.hpp"
#define DXA_GAME_PROBE_FUNCTION ReferenceCombatProbe
#elif defined(DXA_GAME_PROBE_TRAVERSAL)
#include "ReferenceTraversalProbe.hpp"
#define DXA_GAME_PROBE_FUNCTION ReferenceTraversalProbe
#elif defined(DXA_GAME_PROBE_SELECTION)
#include "ReferenceSelectionProbe.hpp"
#define DXA_GAME_PROBE_FUNCTION ReferenceSelectionGameProbe
#elif defined(DXA_GAME_PROBE_AUDIO)
#include "ReferenceAudioProbe.hpp"
#define DXA_GAME_PROBE_FUNCTION ReferenceAudioProbe
#elif defined(DXA_GAME_PROBE_DIAGNOSTICS)
#define DXA_GAME_PROBE_FUNCTION(...) ((void)0)
#else
#error Unknown diagnostic game scenario
#endif
