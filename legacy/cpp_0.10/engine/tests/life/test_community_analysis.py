import unittest,sys,copy
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from analyze_community import audit,distribution
class AnalysisContracts(unittest.TestCase):
 def fixture(self):
  names=['idle','travel','eating','drinking','sleep','rest','leisure','social','private','work','study','other'];t={x:0 for x in names};t.update(sleep=8*3600000,work=8*3600000,leisure=4*3600000,idle=4*3600000)
  a=dict(id=1,alive=True,job=0,money=120,earned=0,food_spent=0,leisure_spent=0,study_spent=0,rent_paid=0,rent_debt=0,promotions=0,study_sessions=0,received=0,forgotten=0,memory_count=0,memories=[],opinions=[],time_ms=t)
  return dict(scenario='normal',seed=42,population=1,alive=1,critical_actor_seconds=0,max_damage=0,hash='fixture'),dict(enabled=True,ms=86400000,actors=[a],utterances=0,deliveries=0,group_deliveries=0,speech_seconds=0,third_party_seconds=0,disclosures=0)
 def test_valid_ledger(self):self.assertEqual(audit(*self.fixture())['invariant_errors'],[])
 def test_double_count_rejected(self):s,c=self.fixture();c['actors'][0]['time_ms']['social']=3600000;self.assertTrue(audit(s,c)['invariant_errors'])
 def test_cash_does_not_appear(self):s,c=self.fixture();c['actors'][0]['money']=999;self.assertTrue(audit(s,c)['invariant_errors'])
 def test_critical_not_hidden_by_alive(self):s,c=self.fixture();s['critical_actor_seconds']=10;self.assertIn('critical_needs_nonzero',audit(s,c)['pilot_warnings'])
 def test_listener_copies_not_speech(self):s,c=self.fixture();c.update(speech_seconds=60,deliveries=10);self.assertEqual(audit(s,c)['speech_minutes_per_person_day'],1)
 def test_empty_not_success(self):s,c=self.fixture();c['actors']=[];self.assertRaises(ValueError,audit,s,c)
 def test_distribution_not_only_average(self):self.assertEqual(distribution([1,1,10])['median'],1)
 def test_dead_not_full_day_denominator(self):s,c=self.fixture();s['alive']=0;c['actors'][0]['alive']=False;c['actors'][0]['time_ms']={k:v/2 for k,v in c['actors'][0]['time_ms'].items()};self.assertEqual(audit(s,c)['person_days'],.5)
if __name__=='__main__':unittest.main(verbosity=2)
