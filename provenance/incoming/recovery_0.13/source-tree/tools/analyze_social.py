#!/usr/bin/env python3
"""Audit recorded thoughts, scoped consent, physical loans and contextual memory.

This checks machine contracts, not psychological realism. Whole-world counts come
from social-state.json; traces may deliberately cover only one actor.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
import math
from pathlib import Path
from analyze_thoughts import analyze


def audit(root: Path) -> dict:
    state = json.loads((root / 'social-state.json').read_text(encoding='utf-8'))
    summary = json.loads((root / 'summary.json').read_text(encoding='utf-8'))
    run = json.loads((root / 'run.json').read_text(encoding='utf-8'))
    thoughts = [json.loads(line) for line in (root / 'thoughts.jsonl').read_text(encoding='utf-8').splitlines() if line.strip()]
    basic = analyze(root / 'thoughts.jsonl')
    errors = list(basic['errors'])
    lookup = defaultdict(dict)
    social_intents = 0
    for t in thoughts:
        if t['method'] in ('social', 'use_object') and t['kind'] == 'intent' and t['basis']:
            social_intents += 1
            basis = lookup[t['actor']].get(t['basis'])
            if basis is None or any(basis.get(k) != t.get(k) for k in ('method','interaction','object','person')):
                errors.append(f"intent {t['actor']}:{t['thought']} does not match chosen scoped forecast")
        if t['method'] == 'social' and t['kind'] == 'forecast':
            if not t['knowledge_source']:
                errors.append(f"forecast {t['actor']}:{t['thought']} has no knowledge source")
            if not (-1 <= t['expected_pleasure'] <= 1 and 0 <= t['expected_acceptance'] <= 1):
                errors.append('forecast range')
        lookup[t['actor']][t['thought']] = t
    counts = Counter()
    matched_answers = 0
    event_ids = set()
    for event in state['events']:
        if event['id'] in event_ids:
            errors.append('duplicate interaction event')
        event_ids.add(event['id'])
        if event['a'] == event['b'] or not event['parent']:
            errors.append('invalid contact scope')
        if event['phase'] in (1,2) and event['answered_ms'] <= event['proposed_ms']:
            errors.append('consent before the receiver has thought')
        if event['phase'] == 2:
            counts[event['interaction']] += 1
            if event['due_ms'] <= event['answered_ms'] or event['due_ms'] > state['ms']:
                errors.append('effect did not take logical time')
        if event['phase'] in (1,2,3) and (not run['trace_actor'] or run['trace_actor'] == event['b']):
            expected = 'social_decline_specific_proposal' if event['phase'] == 3 else 'social_accept_specific_proposal'
            matches = [t for t in thoughts if t['actor'] == event['b'] and t['ms'] == event['answered_ms']
                       and t['person'] == event['a'] and t['interaction'] == event['interaction']
                       and t['object'] == event['object'] and t['kind'] == 'interpret_reply']
            # Unknown methods are a real negative answer, not an accepted action.
            if not any(t['detail'] == expected or (event['phase'] == 3 and t['detail'] == 'social_unknown_interaction') for t in matches):
                errors.append(f"event {event['id']} has no traced receiver answer")
            else:
                matched_answers += 1
    for kind, n in summary['social_events']['completed'].items():
        if kind != 'use_item' and counts[kind] != n:
            errors.append(f'completed count differs: {kind}')
    objects = {o['id']: o for o in state['world_objects']}
    loans = {l['id']: l for l in state['loans']}
    for obj in objects.values():
        if obj['active_loan']:
            loan = loans.get(obj['active_loan'])
            if not loan or loan['returned'] or loan['lender'] != obj['owner'] or loan['borrower'] != obj['holder']:
                errors.append('loan/ownership conservation')
    false_claims = sum(e['phase'] == 2 and e['interaction'] == 'claim_item' and objects[e['object']]['owner'] != e['a'] for e in state['events'])
    return {'scene': run['name'], 'seconds': run['seconds'], 'trace_actor': run['trace_actor'],
            'thought_rows': basic['rows'], 'intents_checked': basic['checked_intents'],
            'social_intents_checked': social_intents, 'scoped_answers_checked': matched_answers,
            'events': len(event_ids), 'completed': dict(counts), 'debug_false_claims': false_claims,
            'max_context_slots': basic['max_slots'], 'max_spent': basic['max_spent'],
            'errors': errors}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('runs', type=Path)
    p.add_argument('--out', type=Path)
    args = p.parse_args()
    try:
        reports = [audit(d) for d in sorted(args.runs.iterdir()) if d.is_dir() and (d/'run.json').exists()]
        if not reports:
            p.error('no run directories')
        data = {'scenes': reports, 'thought_rows': sum(x['thought_rows'] for x in reports),
                'intents_checked': sum(x['intents_checked'] for x in reports),
                'errors': sum(len(x['errors']) for x in reports)}
        text = json.dumps(data, ensure_ascii=False, indent=2) + '\n'
        if args.out:
            args.out.write_text(text, encoding='utf-8')
        else:
            print(text)
        for r in reports:
            print(r['scene'], r['thought_rows'], 'rows', len(r['errors']), 'errors')
        return int(data['errors'] > 0)
    except (OSError, ValueError, KeyError) as exc:
        p.exit(2, f'Cannot verify: {exc}\n')


if __name__ == '__main__':
    raise SystemExit(main())
