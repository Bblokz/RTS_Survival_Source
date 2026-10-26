"""Regression checks for the hard short cap and opt-in Studio category."""
import sys
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]/"ShortsCreator"
sys.path.insert(0,str(ROOT/"Tools"))
import dashboard_model as model

class ParatrooperPresetTests(unittest.TestCase):
    def request(self,**overrides):
        return {"jobId":"SovietParatrooperBattle","setupId":"ParatrooperBattle",**overrides}

    def test_faction_armies_and_duration(self):
        result=model.compose(ROOT,self.request())
        self.assertEqual(result["category"],"paratrooper")
        self.assertEqual(result["summary"]["seconds"],120)
        segment=result["job"]["Segments"][0]
        self.assertEqual(segment["ParatrooperAbility"],"GA_SovietParatroopers")
        self.assertEqual(segment["ParatrooperIntroSeconds"],2)
        # Whole Soviet lineup and the drop share player 2, opposed by player 1.
        self.assertEqual(result["job"]["UnitOwningPlayer"],2)
        self.assertEqual(segment["TargetOwningPlayer"],1)
        self.assertEqual(segment["ParatrooperCameraMove"]["DurationSeconds"],30)
        self.assertEqual(segment["ParatrooperCameraMove"]["EndOffset"]["Z"],1100)
        self.assertLess(segment["ParatrooperCameraMove"]["EndOffset"]["Y"],-1100)
        self.assertEqual(segment["Camera"]["Height"],2200)
        continuation=segment["ParatrooperCameraMove"]["ContinuationPoints"]
        self.assertEqual(sum(point["DurationSeconds"] for point in continuation),48)
        self.assertEqual(continuation[-1]["Offset"],{"X":5200,"Y":1000,"Z":2200})
        self.assertEqual(continuation[-1]["FocalLength"],segment["Camera"]["FocalLength"])
        self.assertEqual(len(segment["BattleEnemyClassPaths"]),16)
        self.assertEqual(len(segment["BattleAllyClassPaths"]),10)
        self.assertNotIn('_Soviet',segment['ClassPath'])
        self.assertEqual(sum('Rus_Squad_PTRS.' in path for path in segment['BattleAllyClassPaths']),3)
        self.assertEqual(sum('Squad_Ger_PanzerGrenadiers.' in path for path in segment['BattleEnemyClassPaths']),3)
        self.assertEqual(sum("Hazmat" in path for path in segment["BattleAllyClassPaths"]),2)
        self.assertEqual(sum("GerPit_Flak37" in path for path in segment["BattleEnemyClassPaths"]),2)
        self.assertIn("Rus_BXP_ML20Pit",segment["BattleAllyClassPaths"][5])
        self.assertLess(segment["BattleAllyOffsets"][5]["X"],0)
        self.assertTrue(all(offset["X"]==800 for offset in segment["BattleAllyOffsets"][6:9]))
        self.assertTrue(all(offset["X"]==-800 for offset in segment["BattleEnemyOffsets"][6:9]))
        self.assertIn('LeFH18',segment['BattleEnemyClassPaths'][9])
        self.assertEqual(segment['BattleEnemyOffsets'][9]['X'],3200)
        self.assertEqual(sum('PanzerIV_G.' in path for path in segment['BattleEnemyClassPaths']),2)
        self.assertEqual(sum('Ger_Vehicle_PantherD.' in path for path in segment['BattleEnemyClassPaths']),1)
        self.assertEqual(sum('Squad_Ger_HazmatVultures.' in path for path in segment['BattleEnemyClassPaths']),6)
        self.assertEqual(len({point['Y'] for point in segment['BattleEnemyOffsets'][-6:]}),6)
        self.assertTrue(all(point['X']==-1050 for point in segment['BattleEnemyOffsets'][-6:]))
        self.assertIn('Rus_Vehicle_IS-1_Soviet.',segment['BattleAllyClassPaths'][-1])
        self.assertIn('1× IS-1 (HEAT)',segment['Caption1'])
        self.assertEqual(len(segment["Caption1"].split("|")),8)
        self.assertEqual(len(segment["Caption2"].split("|")),8)

    def test_cap_survives_global_and_advanced_overrides(self):
        for request in [self.request(controls={"battleSeconds":121}),
                        self.request(controls={"segments":[{"index":0,"patch":{"SlotSeconds":150,"StopAtSeconds":150}}]}),
                        self.request(controls={"segments":[{"index":0,"patch":{"CutLeadFrames":30}}]})]:
            with self.subTest(request=request),self.assertRaises(ValueError):
                model.compose(ROOT,request)

    def test_custom_armies_and_german_drop_remain_supported(self):
        result=model.compose(ROOT,self.request(controls={"segment":{"ParatrooperAbility":"GA_GerParatroopers"},"battleSeconds":60}))
        self.assertEqual(result["job"]["Segments"][0]["ParatrooperAbility"],"GA_GerParatroopers")
        self.assertEqual(result["summary"]["seconds"],60)

    def test_isolated_biome_and_original_categories(self):
        self.assertEqual(model.allowed_biomes("paratrooper"),["ParatrooperMeadow"])
        self.assertNotIn("ParatrooperMeadow",model.allowed_biomes("battle"))
        self.assertEqual(model.compose(ROOT,{"jobId":"BattleDesertPzIVvsT34","setupId":"TankBattleDesert"})["category"],"battle")

if __name__=="__main__":
    unittest.main(verbosity=2)
