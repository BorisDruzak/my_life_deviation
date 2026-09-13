"""Checks of the proposed equations only; not tests of the game's C++ runtime."""
import math
import unittest
from norm_math_reference import (
    BinaryEstimate, EvidenceBook, Ledger, Term, approval, evidence_weight,
    decay, personal_update, sanction_cost, audience_value, group_significance,
    norm_tension, hysteresis, buffer_cost, pv, squash, combined_acceptance,
)

class NormMathTests(unittest.TestCase):
    def test_01_empty_is_unknown(self):
        e = BinaryEstimate(); self.assertFalse(e.known); self.assertEqual(e.p, .5); self.assertEqual(e.coverage, 0)
    def test_02_eight_of_ten(self):
        e = BinaryEstimate(8, 2); self.assertAlmostEqual(e.p, .75); self.assertAlmostEqual(e.coverage, 10/14)
    def test_03_uniform_scaling_changes_certainty_not_ratio_blindly(self):
        self.assertLess(BinaryEstimate(.8,.2).p, BinaryEstimate(8,2).p)
    def test_04_approval_has_three_outcomes(self):
        p = approval((5,2,1)); self.assertEqual(p, (6/11,3/11,2/11)); self.assertNotEqual(1-p[0], p[1])
    def test_05_explicit_prior_is_known(self):
        self.assertTrue(BinaryEstimate(prior_positive=8, prior_negative=2, explicit_prior=True).known)
    def test_06_weight_only_once(self):
        self.assertAlmostEqual(evidence_weight(.8,.7,.6,.5), .168)
    def test_07_decay_half_life(self):
        self.assertEqual(decay(8,30,30),4)
    def test_08_decay_partition(self):
        self.assertAlmostEqual(decay(decay(8,10,30),20,30),decay(8,30,30),places=14)
    def test_09_no_clock_no_decay(self):
        self.assertEqual(decay(8,0,30),8)
    def test_10_personal_principle_existing_equation(self):
        self.assertAlmostEqual(personal_update(.8,.2,.5,1,1), .7940299002495008, places=14)
    def test_11_no_new_reason_no_personal_learning(self):
        self.assertEqual(personal_update(.8,.2,.5,0,1),.8)
    def test_12_sanction(self):
        self.assertAlmostEqual(sanction_cost(.4,.8,.5,.6),.096,places=14)
    def test_13_unknown_sanction_is_not_zero(self):
        self.assertIsNone(sanction_cost(None,.8,.5,.6))
    def test_14_invisible_audience(self):
        self.assertEqual(audience_value(1,0,.8,.2,.5,.5),0)
    def test_15_unimportant_group(self):
        self.assertEqual(audience_value(0,1,.8,.2,.5,.5),0)
    def test_16_group_goals_not_personality(self):
        self.assertAlmostEqual(group_significance([(0.8,0.9),(0.7,0.2)]),.72)
    def test_17_no_group_goal(self):
        self.assertEqual(group_significance([]),0)
    def test_18_tension_does_not_override_hysteresis(self):
        self.assertAlmostEqual(norm_tension(.8,.9,.75,.4),.216)
        self.assertTrue(hysteresis(.12,False)); self.assertTrue(hysteresis(.10,True)); self.assertFalse(hysteresis(.07,True))
    def test_19_hysteresis_repeat(self):
        self.assertFalse(hysteresis(.10,False)); self.assertTrue(hysteresis(.10,True))
    def test_20_buffer_spending_above_reserve(self):
        self.assertEqual(buffer_cost(300,100,95,.8),0)
    def test_21_buffer_new_shortfall(self):
        self.assertAlmostEqual(buffer_cost(220,200,95,.8),.3)
    def test_22_buffer_already_short_counts_increment(self):
        self.assertAlmostEqual(buffer_cost(50,100,20,.8),.16)
    def test_23_subjective_money_does_not_enable_unaffordable_payment(self):
        with self.assertRaises(ValueError): buffer_cost(20,100,35,.8)
    def test_24_discount_once(self):
        self.assertAlmostEqual(pv(1,.8,5,.035),.8*math.exp(-.175))
    def test_25_specific_experience_displaces_group(self):
        self.assertAlmostEqual(combined_acceptance(.2,1,.9,.8,.95,1),.2)
    def test_26_self_is_weak_prior_not_extra_reward(self):
        self.assertAlmostEqual(combined_acceptance(.2,0,.6,0,.2,1),.5)
    def test_27_duplicate_known_root_no_reinforcement(self):
        b = EvidenceBook(); b.apply(10,1,1,.8,0,1); b.apply(10,1,1,.8,0,2)
        self.assertAlmostEqual(b.estimate(0).positive,.8)
    def test_28_revision_replaces_not_adds(self):
        b = EvidenceBook(); b.apply(10,1,1,1,0,1); b.apply(10,2,0,1,0,2)
        e=b.estimate(0); self.assertEqual(e.positive,0); self.assertEqual(e.negative,1)
    def test_29_delivery_duplicate(self):
        b=EvidenceBook(); b.apply(10,1,1,1,0,1); self.assertEqual(b.apply(11,1,0,1,0,1),'duplicate_delivery')
        self.assertEqual(b.estimate(0).positive,1)
    def test_30_inconsistent_same_revision_rejected(self):
        b=EvidenceBook(); b.apply(10,1,1,1,0,1)
        with self.assertRaises(ValueError): b.apply(10,1,0,1,0,2)
        self.assertEqual(b.estimate(0).positive,1)
    def test_31_invalid_input_does_not_change_evidence(self):
        b=EvidenceBook(); b.apply(10,1,1,1,0,1)
        with self.assertRaises(ValueError): b.apply(10,2,float('nan'),1,0,2)
        self.assertEqual(b.estimate(0).positive,1)
    def test_32_ledger_duplicate_rejected(self):
        x=Ledger(); t=Term('approval_B',-.2); x.insert(t)
        with self.assertRaises(ValueError): x.insert(t)
        self.assertAlmostEqual(x.value(.03),-.2)
    def test_33_ledger_present_value_not_discounted_again(self):
        x=Ledger(); x.insert(Term('future',pv(1,.8,5,.035),present_value=True))
        self.assertAlmostEqual(x.value(.035),.8*math.exp(-.175))
    def test_34_ledger_single_squash(self):
        x=Ledger(); x.insert(Term('benefit',.8)); x.insert(Term('money',-.2));
        self.assertAlmostEqual(x.score(0),.6/1.6)
    def test_35_validation(self):
        for bad in (float('nan'),float('inf'),-0.1,1.1):
            with self.assertRaises(ValueError): evidence_weight(bad,1,1,1)
        with self.assertRaises(ValueError): decay(1,-1,30)
        with self.assertRaises(ValueError): approval((-1,0,0))
        with self.assertRaises(ValueError): BinaryEstimate(prior_positive=0)
    def test_36_norm_tension_is_not_in_ledger(self):
        x=Ledger(); x.insert(Term('physical',.5)); before=x.value(.035)
        norm_tension(1,1,1,1)
        self.assertEqual(x.value(.035),before)

if __name__ == '__main__':
    unittest.main(verbosity=2)
