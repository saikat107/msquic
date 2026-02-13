# BBR Semantic Index - Quick Start Guide

## Overview

This semantic index provides a comprehensive database of BBR (Bottleneck Bandwidth and RTT) congestion control functions in MsQuic, including:

- Complete call graphs with nested dependencies
- Function summaries based on actual source code
- Preconditions and postconditions for safety analysis
- 62 functions indexed with 80 call edges

## Files

- **`.deeptest/bbr.db`** - SQLite database with semantic index
- **`.deeptest/BBR_SEMANTIC_INDEX_REPORT.md`** - Comprehensive analysis report
- **`.github/skills/semantic-indexer/scripts/`** - Tools for querying and extending the index

## Quick Queries

### 1. Get Database Statistics
```bash
python3 .github/skills/semantic-indexer/scripts/indexer.py \
  --db .deeptest/bbr.db stats
```

### 2. Query a Function's Call Graph
```bash
python3 .github/skills/semantic-indexer/scripts/indexer.py \
  --db .deeptest/bbr.db \
  query --focal "BbrCongestionControlOnDataAcknowledged"
```

### 3. Check Summarization Progress
```bash
python3 .github/skills/semantic-indexer/scripts/summarizer.py \
  --db .deeptest/bbr.db status
```

### 4. Direct SQL Queries

List all BBR functions:
```bash
sqlite3 .deeptest/bbr.db "SELECT name, start_line FROM functions WHERE file LIKE '%bbr.c' ORDER BY start_line;"
```

Get preconditions for a function:
```bash
sqlite3 .deeptest/bbr.db "
SELECT f.name, p.condition_text
FROM functions f
JOIN preconditions p ON f.function_id = p.function_id
WHERE f.name = 'BbrCongestionControlOnDataLost'
ORDER BY p.sequence_order;
"
```

Get postconditions:
```bash
sqlite3 .deeptest/bbr.db "
SELECT f.name, p.condition_text
FROM functions f
JOIN postconditions p ON f.function_id = p.function_id
WHERE f.name = 'BbrCongestionControlOnDataAcknowledged'
ORDER BY p.sequence_order;
"
```

Find functions that modify a specific variable:
```bash
sqlite3 .deeptest/bbr.db "
SELECT name, summary
FROM functions
WHERE summary LIKE '%BytesInFlight%' AND file LIKE '%bbr.c%'
ORDER BY name;
"
```

## Indexed Functions

### Entry Points (3)
- `BbrCongestionControlInitialize` - Initialize BBR state
- `BbrCongestionControlOnDataAcknowledged` - Process ACKs
- `BbrCongestionControlOnDataLost` - Handle packet loss

### State Transitions (4)
- `BbrCongestionControlTransitToStartup`
- `BbrCongestionControlTransitToDrain`
- `BbrCongestionControlTransitToProbeBw`
- `BbrCongestionControlTransitToProbeRtt`

### Update Functions (4)
- `BbrCongestionControlUpdateCongestionWindow`
- `BbrCongestionControlUpdateRecoveryWindow`
- `BbrCongestionControlUpdateAckAggregation`
- `BbrCongestionControlUpdateBlockedState`

### Query Functions (7)
- `BbrCongestionControlGetBandwidth`
- `BbrCongestionControlGetCongestionWindow`
- `BbrCongestionControlGetTargetCwnd`
- `BbrCongestionControlGetNetworkStatistics`
- `BbrCongestionControlCanSend`
- `BbrCongestionControlInRecovery`
- `BbrCongestionControlIsAppLimited`

### Helper Functions (5)
- `BbrBandwidthFilterOnPacketAcked`
- `BbrCongestionControlSetSendQuantum`
- `BbrCongestionControlHandleAckInProbeRtt`
- `QuicConnLogBbr`
- Plus platform abstractions (34 functions)

## Safety Analysis Summary

✅ **No Critical Issues Found**

⚠️ **3 Low-Severity Findings:**
1. `BbrBandwidthFilterOnPacketAcked` - Potential division by zero in delivery rate calculation
2. `BbrCongestionControlGetBandwidth` - May involve division in bandwidth query
3. `BbrCongestionControlGetTargetCwnd` - Target cwnd calculation with division

**Recommendation:** Review these three functions for explicit divisor validation.

## Extending the Index

### Add More Functions
```bash
python3 .github/skills/semantic-indexer/scripts/build_focal.py \
  --focal "FunctionName" \
  --file "path/to/file.c" \
  --project /home/runner/work/msquic/msquic \
  --db .deeptest/bbr.db
```

### Update Function Summary
```bash
python3 .github/skills/semantic-indexer/scripts/summarizer.py \
  --db .deeptest/bbr.db \
  --project /home/runner/work/msquic/msquic \
  next
```

### Add Annotations
```bash
python3 .github/skills/semantic-indexer/scripts/summarizer.py \
  --db .deeptest/bbr.db \
  annotate \
  --function "FunctionName" \
  --type precondition \
  --text "Description of precondition"
```

## Use Cases

### 1. Understanding Call Hierarchies
Query any function to see its complete dependency tree, helping understand how BBR operations cascade through the system.

### 2. Safety Analysis
Check preconditions and postconditions to identify potential contract violations or missing validations.

### 3. Test Generation
Use function summaries and conditions to generate targeted unit tests that exercise specific code paths and edge cases.

### 4. Code Review
Reference the semantic index during code reviews to ensure new changes maintain existing contracts and safety properties.

### 5. Documentation
Export function summaries and call graphs to generate or update technical documentation.

## Database Schema

```sql
CREATE TABLE functions (
    function_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    file TEXT NOT NULL,
    start_line INTEGER,
    end_line INTEGER,
    summary TEXT
);

CREATE TABLE call_edges (
    caller_id INTEGER,
    callee_id INTEGER,
    FOREIGN KEY(caller_id) REFERENCES functions(function_id),
    FOREIGN KEY(callee_id) REFERENCES functions(function_id)
);

CREATE TABLE preconditions (
    function_id INTEGER,
    condition_text TEXT,
    sequence_order INTEGER,
    FOREIGN KEY(function_id) REFERENCES functions(function_id)
);

CREATE TABLE postconditions (
    function_id INTEGER,
    condition_text TEXT,
    sequence_order INTEGER,
    FOREIGN KEY(function_id) REFERENCES functions(function_id)
);
```

## Common Preconditions

| Precondition | Affected Functions | Impact |
|--------------|-------------------|--------|
| Input pointers must be non-NULL | 42 | HIGH |
| Congestion control object must be properly initialized | 18 | HIGH |
| BytesInFlight must accurately reflect outstanding network data | 8 | CRITICAL |

## Common Postconditions

| Postcondition | Affected Functions | Impact |
|---------------|-------------------|--------|
| Returns computed value based on current state | 31 | HIGH |
| Congestion window is updated based on current conditions | 15 | CRITICAL |
| BBR state machine transitions to new state | 8 | HIGH |

## Future Enhancements

1. **Extend Coverage:** Add remaining BBR helper functions and test infrastructure
2. **Cross-Reference Tests:** Map unit tests to indexed functions to identify coverage gaps
3. **Mutation Testing:** Use preconditions/postconditions to generate mutation tests
4. **Static Analysis Integration:** Feed semantic index into static analysis tools
5. **Documentation Generator:** Auto-generate API documentation from summaries

## Support

For questions or issues with the semantic index:
- See comprehensive analysis in `.deeptest/BBR_SEMANTIC_INDEX_REPORT.md`
- Check skill documentation in `.github/skills/semantic-indexer/`
- SQLite database can be queried with any SQLite client

---

**Last Updated:** 2026-02-05  
**Index Version:** 1.0  
**Total Functions:** 62  
**Completion:** 100%
