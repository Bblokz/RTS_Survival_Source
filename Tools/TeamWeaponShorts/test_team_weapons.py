"""Check the paired-shot contract without starting Unreal."""
import copy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3] / 'ShortsCreator'
sys.path.insert(0, str(ROOT / 'Tools'))
import dashboard_model as model

PRESET = 'TeamWeapons_Sdkfz_Pak40_Flak88_vs_T3476_SU76'


class TeamWeaponCompositionTests(unittest.TestCase):
    def compose(self, **extra):
        return model.compose(ROOT, dict(jobId=PRESET, setupId='TeamWeaponsBattle', **extra))

    def test_reusable_setup_keeps_exact_intro_and_both_sets(self):
        result = self.compose(biome='TeamWeapons')
        job = result['job']
        intro, battle = job['Segments']
        self.assertEqual(result['category'], 'teamweapons')
        self.assertEqual(intro['SlotSeconds'], 2)
        self.assertEqual(intro['StopAtSeconds'], 2)
        self.assertEqual([intro['StageActorName'], battle['StageActorName']], ['TeamWeaponDesert', 'TeamWeaponIndustrial'])
        self.assertEqual(sum(s['SlotSeconds'] for s in job['Segments']), 120)
        self.assertEqual((job['ResolutionX'], job['ResolutionY']), (1080, 1920))

    def test_first_lineup_has_native_crews_and_regular_soviet_vehicles(self):
        intro, battle = self.compose()['job']['Segments']
        crews = intro['TeamWeapons']['SquadClassPaths']
        self.assertEqual(sum('Pak40' in path for path in crews), 3)
        self.assertEqual(sum('Flak88Wheels' in path for path in crews), 1)
        enemies = battle['BattleEnemyClassPaths']
        self.assertEqual(sum('Rus_Vehicle_T3476.' in path for path in enemies), 3)
        self.assertEqual(sum('Rus_Vehicle_SU76.' in path for path in enemies), 1)
        self.assertFalse(any('_Soviet' in path for path in enemies))

    def test_rejects_missing_or_reversed_shot(self):
        for selected in ([0], [1], [1, 0]):
            with self.subTest(selected=selected), self.assertRaises(ValueError):
                self.compose(selectedSegments=selected)

    def test_rejects_crew_count_mismatch_and_bare_gun(self):
        job = self.compose()['job']
        for crews in ([], ['/Game/Some/Gun.Gun_C'] * 4):
            invalid = copy.deepcopy(job)
            for shot in invalid['Segments']:
                shot['TeamWeapons']['SquadClassPaths'] = crews
            with self.subTest(crews=crews), self.assertRaises(ValueError):
                model.validate(invalid, 'teamweapons')

    def test_duration_control_cannot_trim_intro_or_exceed_cap(self):
        self.assertEqual(self.compose(controls={'battleSeconds': 45})['job']['Segments'][0]['StopAtSeconds'], 2)
        for controls in ({'convoySeconds': 3}, {'battleSeconds': 119}, {'job': {'CutLeadFrames': 3}, 'segment': {'CutLeadFrames': 3}}, {'job': {'WarmupSeconds': 2}}):
            with self.subTest(controls=controls), self.assertRaises(ValueError):
                self.compose(controls=controls)

    def test_rejects_vehicle_and_crew_discontinuity(self):
        for field, value in (('ClassPath', '/Game/Test/Other.Other_C'), ('TeamWeapons', {'SquadClassPaths': [self.compose()['job']['Segments'][0]['TeamWeapons']['SquadClassPaths'][0]] * 4})):
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.compose(controls={'segments': [{'index': 1, 'patch': {field: value}}]})

    def test_rejects_same_player_and_wrong_biome(self):
        with self.assertRaises(ValueError):
            self.compose(controls={'segment': {'TargetOwningPlayer': 1}})
        with self.assertRaises(ValueError):
            self.compose(biome='Meadow')


if __name__ == '__main__':
    unittest.main(verbosity=2)
