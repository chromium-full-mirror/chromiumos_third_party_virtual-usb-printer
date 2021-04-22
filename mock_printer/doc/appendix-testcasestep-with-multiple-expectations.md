# Appendix: TestCaseStep With Multiple Expectations

Each `TestCaseStep` associates a `Cardinality` with a set of
`ExpectationWithResponse`s. `virtual-usb-printer` matches incoming
messages greedily, so test authors must structure their matchers
carefully.

## Matcher order matters

***promo
Using `Cardinality::Type::REPEATED` constrains the test author to
providing exactly one `ExpectationWithResponse` in the `TestCaseStep`.
The fields in `Cardinality::Count` then provide upper, lower, or exact
bounds on how many times the contained `Expectation` must be fulfilled.
***

When processing a `TestCaseStep` with multiple
`ExpectationWithResponse`s and `Cardinality::Type::ALL_IN_ANY_ORDER` or
`Cardinality::Type::SOME_OF`, `virtual-usb-printer` will react to each
incoming message by looking for the first matching
`ExpectationWithResponse` and mark it as fulfilled.

Suppose you have a `TestCaseStep`:

```none
cardinality {
  type: SOME_OF
  count { exactly: 3 }
}
expectation_with_response { expectation { # "expectation 1"
  # match A, B, or C
} }
expectation_with_response { expectation { # "expectation 2"
  # match B
} }
expectation_with_response { expectation { # "expectation 3"
  # match X
} }
expectation_with_response { expectation { # "expectation 4"
  # match Y
} }
```

Suppose that `virtual-usb-printer` then receives the following messages:

1.  Message `X`. This matches expectation 3, which is removed from
    consideration for future matches. 1 match of 3 total is fulfilled.
1.  Message `B`. This matches _both_ expectation 1 and expectation 2.
    `virtual-usb-printer` greedily uses the first reachable expectation
    and removes expectation 1 from consideration for future matches. 2
    matches of 3 total are fulfilled.
1.  Message `C`. This _would_ match expectation 1, but it was stricken
    from consideration by the previous match. No other expectations
    in this `TestCaseStep` match message `C`.
1.  This `TestCaseStep` did not reach the prescribed `Cardinality`,
    so it is unfulfilled. The `TestCase` fails.

*** aside
The sequence of messages above would also fail given
`Type::ALL_IN_ANY_ORDER`, which would demand that all four expectations
be fulfilled; the matching process follows the same rules.
***

## TestCaseSteps with fuzzy Cardinality::Count

When processing a `TestCaseStep` with `Cardinality::Type::SOME_OF`,
test writers must provide `Cardinality::Count`. When the `Count` is
fuzzy (e.g. specifies `at_least` and `at_most`, not `exactly`),
`virtual-usb-printer` still takes a greedy match approach to determining
when a `TestCaseStep` is complete.

Suppose you have a `TestCaseStep`:

```none
cardinality {
  type: SOME_OF
  count {
    at_least: 2
    at_most: 4
  }
}
expectation_with_response { expectation { # "expectation 1"
  # match A, B, or C
} }
expectation_with_response { expectation { # "expectation 2"
  # match B
} }
expectation_with_response { expectation { # "expectation 3"
  # match X
} }
expectation_with_response { expectation { # "expectation 4"
  # match Y
} }
```

Suppose that `virtual-usb-printer` then receives the following messages:

1.  Message `Y`. This matches expectation 4, which is removed from
    consideration for future matches. We have 1 match now.
2.  Message `C`. This matches expectation 1, which is removed from
    consideration for future matches. We have 2 matches now. Since
    this would satisfy `Cardinality::Count::at_least == 2`, this current
    `TestCaseStep` is tentatively fulfilled; `virtual-usb-printer` is
    _free to_ move onto the next `TestCaseStep`, but will continue
    running out the current `TestcaseStep` if more matching messages
    arrive.

Consider this possibility for the next inbound messages:

3.  Message `B`. This matches expectation 2, which is removed from
    consideration for future matches. We have 3 matches now. This
    still satisfies the bounds provided by the `Cardinality` and the
    present `TestCaseStep` is still tentatively fulfilled.
4.  Message `Z`. This matches none of the expectations in the present
    `TestCaseStep`, so `virtual-usb-printer` finalizes the tentative
    fulfillment of this `TestCaseStep` and seeks out a matcher in the
    following `TestCaseStep`.

Consider this alternate possibility for the next inbound messages:

3.  Message `Z`. Same as step 4 in the previous scenario; this
    `TestCaseStep` graduates from tentative fulfillment (2 total
    matches) and `virtual-usb-printer` seeks out a match in the
    following `TestCaseStep`.
