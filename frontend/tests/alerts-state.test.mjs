import assert from "node:assert/strict";
import test from "node:test";

import { withoutArchivedRule } from "../src/lib/alerts-state.mjs";


test("archived rule disappears immediately without mutating prior state", () => {
    const rules = [
        { id: 1, name: "Keep" },
        { id: 2, name: "Archive" },
    ];

    const updated = withoutArchivedRule(rules, 2);

    assert.deepEqual(updated, [{ id: 1, name: "Keep" }]);
    assert.equal(rules.length, 2);
});
