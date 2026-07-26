/**
 * @template {{ id: number }} T
 * @param {T[]} rules
 * @param {number} archivedRuleId
 * @returns {T[]}
 */
export function withoutArchivedRule(rules, archivedRuleId) {
    return rules.filter((rule) => rule.id !== archivedRuleId);
}
