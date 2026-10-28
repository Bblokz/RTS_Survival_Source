"""Regression checks for automatic round labels on the actual paratrooper render path."""
import contextlib
import copy
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

PROJECT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PROJECT / 'ShortsCreator/Tools'))
import dashboard_model
import forge_rounds
import run_shorts


class BalancedParatrooperTests(unittest.TestCase):
    def jobs(self):
        return [json.loads(path.read_text(encoding='utf-8'))
                for path in sorted((PROJECT / 'ShortsCreator/Jobs').glob('ParatrooperFair0*.json'))]

    def test_three_private_jobs_and_equal_armour_counts(self):
        jobs = self.jobs()
        self.assertEqual(len(jobs), 3)
        self.assertEqual([job['Segments'][0]['ParatrooperFormation']['OwningPlayer'] for job in jobs], [1, 2, 2])
        for job in jobs:
            segment = job['Segments'][0]
            resolved = dashboard_model.compose(PROJECT / 'ShortsCreator', {
                'jobId': job['JobName'], 'setupId': 'ParatrooperBattle', 'advanced': job})
            self.assertEqual(resolved['category'], 'paratrooper')
            self.assertEqual(resolved['warnings'], [])
            self.assertEqual((job['ResolutionX'], job['ResolutionY'], job['Fps']), (1080, 1920, 60))
            self.assertEqual(segment['SlotSeconds'], 120)
            self.assertEqual(segment['StopAtSeconds'], 120)
            self.assertEqual(segment['ParatrooperIntroSeconds'], 2)
            self.assertEqual(len(segment['ParatrooperFormation']['Passes']), 2)
            for classes in [[segment['ClassPath']] + segment['BattleAllyClassPaths'], segment['BattleEnemyClassPaths']]:
                self.assertEqual(sum('GroundVehicles/' in path for path in classes), 5)
                self.assertEqual(sum('BuildingExpansions/' in path for path in classes), 1)
            direction = 1 if segment['ParatrooperFormation']['OwningPlayer'] == 1 else -1
            for flight in segment['ParatrooperFormation']['Passes']:
                self.assertGreater(direction * flight['AircraftStartOffset']['X'], 0)
            self.assertEqual(len(segment['BattleAllyClassPaths']), len(segment['BattleAllyOffsets']))
            self.assertEqual(len(segment['BattleEnemyClassPaths']), len(segment['BattleEnemyOffsets']))

    def test_actual_round_behaviours_cached_and_regular_units_untagged(self):
        table = forge_rounds.load_table(str(PROJECT))
        self.assertFalse(forge_rounds._behaviours_changed(str(PROJECT), table))
        for job in self.jobs():
            packages = [forge_rounds.class_to_package(path) for path in job['UnitLabels']]
            self.assertFalse(forge_rounds._stale(str(PROJECT), table, packages))
            for class_path, label in job['UnitLabels'].items():
                expected = forge_rounds.rounds_for(table, class_path)
                self.assertEqual('(HEAT)' in label, 'HEAT' in expected, label)
                self.assertEqual('(APCR)' in label, 'APCR' in expected, label)

    def test_mixed_regular_heat_columns_and_idempotency(self):
        job = self.jobs()[0]
        original = copy.deepcopy(job)
        segment = job['Segments'][0]
        tokens = segment['Caption1'].split('|')
        self.assertNotIn('HEAT', tokens[1])
        self.assertIn('2× T-34-85 (HEAT)', tokens[2])
        messages = []
        forge_rounds.annotate_job(job, str(PROJECT), log=messages.append)
        self.assertEqual(job, original)
        self.assertFalse(any('WARNING' in message for message in messages), messages)
        heavy = self.jobs()[1]['Segments'][0]['Caption1']
        self.assertIn('1× IS-1 (HEAT)', heavy)
        self.assertIn('1× SU-85 (APCR)', heavy)
        self.assertIn('1× IS-2 |', heavy)
        self.assertIn('2× T-70 (APCR)', self.jobs()[2]['Segments'][0]['Caption1'])

    def test_launcher_annotates_paratroopers_before_native_job_snapshot(self):
        job = self.jobs()[0]
        job['Segments'][0]['Caption1'] = job['Segments'][0]['Caption1'].replace(' (HEAT)', '')
        job['UnitLabels'] = {path: label.replace(' (HEAT)', '') for path, label in job['UnitLabels'].items()}
        process = Mock(returncode=0)
        process.poll.return_value = 0
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'Input.json'
            source.write_text(json.dumps(job), encoding='utf-8')
            with patch.object(run_shorts.subprocess, 'Popen', return_value=process), \
                 patch.object(run_shorts, 'read_done', return_value=True), contextlib.redirect_stdout(io.StringIO()):
                _, completed = run_shorts.launch(str(source), directory, None, True)
            exported = json.loads((Path(directory) / 'Job.json').read_text(encoding='utf-8'))
            self.assertTrue(completed)
            self.assertIn('2× T-34-85 (HEAT)', exported['Segments'][0]['Caption1'])
            self.assertNotIn('HEAT', exported['Segments'][0]['Caption1'].split('|')[1])
            self.assertEqual(exported['Segments'][0]['ParatrooperFormation']['OwningPlayer'], 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
