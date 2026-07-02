# Phrasing Mode Design

Status note: this is historical design material for a larger phrase grammar.
The current implementation is intentionally narrower: initial-verb `be`
phrases using `PW`, optional right-hand `D` for `was`, and the mnemonic
right-hand tail bank documented in `docs/phrasing-tutorial.md`.

This document sketches a Stoin phrasing system inspired by `~/code/jeff-phrasing` and the practice UI in `~/code/phrasing-trainer`.

The core idea is to make phrasing a held pedal mode. When the phrase pedal is down during a stroke, Stoin routes that stroke through a phrase engine instead of the normal dictionary stack. That gives us a separate namespace for phrase grammar without fighting Lapwing or user dictionary conflicts.

## Goals

- Generate useful phrases such as `it is a`, `he saw it`, `he believed her`, and `they didn't go`.
- Keep the first version small enough to practice immediately.
- Preserve a path toward most of Jeff Phrasing's grammar: starters, auxiliaries, negation, tense, have/be structures, modifiers, verbs, and suffix words.
- Treat Jeff Phrasing's stroke allocation as reference material, not as a constraint. Pedal mode gives Stoin its own namespace.
- Add more suffix/object support than Jeff Phrasing where the stroke space allows it.
- Keep pedal handling inside the platform layer so macOS and Linux can share the same steno/phrasing core.
- Make phrase output participate in normal Stoin translation history, spacing, undo, tracing, and timing.

## Non-Goals For The First Pass

- Full reverse lookup.
- A complete trainer UI inside Stoin.
- Lua plugins as the first implementation.
- Perfect one-stroke coverage for every pronoun/article suffix.
- Linux pedal support before the macOS path proves the core model.

## Existing Reference Model

Jeff Phrasing has simple and full forms. Stoin phrasing should ignore Jeff's simple form and design one full-form grammar.

The full-form shape is:

```text
starter + auxiliary + optional not + phrase structure + verb + optional verb suffix + tense
```

Examples from Jeff:

```text
KPWH-BT   -> it is a
KWHR-SZ   -> he saw
KWHR-BLD  -> he believed
TWH*GD    -> they didn't go
SWRO*FGTD -> I shouldn't have gone to
```

Some target phrases already fall out of the Jeff model:

```text
KPWH-BT      -> it is a
TWH*GD       -> they didn't go
STKPWHR-BTD  -> was a
```

The initial suffix gap is phrases like:

```text
KWHR-SZ  + <it>  -> he saw it
KWHR-BLD + <her> -> he believed her
STKPWHR-BD + <it> -> was it
```

The current Jeff engine parses strokes with this shape:

```text
(left starter)(A/O auxiliary bits)-(* negation)(E/U structure bits)(F have bit)(right-hand verb/tense/suffix bits)
```

In Stoin, this should be parsed from stroke bitsets directly rather than with regexes over outline strings.

Important caveat: Jeff Phrasing lives inside Plover's normal dictionary namespace, so a lot of its shape is collision management. Stoin's phrase pedal sidesteps normal dictionary collisions. We can keep the grammar and conjugation ideas while replacing the stroke allocation with a denser Stoin-native layout.

## Stoin-Native Pedal Namespace

Pedal phrase mode should be allowed to be much more abstract than Jeff Phrasing.

With a phrase pedal held, conflicts with normal dictionary entries disappear. The only remaining conflicts are internal to the phrase grammar. That changes the design pressure:

- Jeff uses 7 left-side starter keys for about 13 full-form starters.
- Those 7 keys can encode up to 128 states if we include the empty chord, or 127 non-empty starter codes.
- The right hand has enough combinations for far more verbs than Jeff currently uses. Even a conservative 9-key verb bank is 512 possible bit patterns.
- The thumb and star bank can select grammar, tense, negation, auxiliary, structure, or suffix banks.

Not every theoretical bit pattern is ergonomic, and some combinations may be hard to press on real machines. Still, the available namespace is much larger than Jeff's Plover-safe allocation uses.

So Stoin should not try to preserve Jeff's mnemonic map where it is weak. A better split is:

```text
phrase pedal + starter code + grammar/state code + verb code + suffix code
```

The phrase engine can still generate Jeff-like grammar:

```text
starter: he
tense: past
verb: see
suffix: it
=> he saw it
```

But the stroke does not need to be `KWHR-SZ` plus a suffix. It can be any stable phrase-mode code that is ergonomic and collision-free inside the phrase layer.

### Phase 0: Layout Contract

Before implementing or practicing phrase strokes, freeze a layout contract.

The contract should specify:

- which physical keys belong to each phrase field,
- which fields are abstract codebooks,
- which key meanings are mnemonic and should remain stable,
- which banks are reserved for future expansion,
- how empty-subject and normal subject starters behave inside the single full form,
- which space is reserved for future interrogative or connective phrase families.

No practice deck should be generated until this contract is agreed enough that early strokes are unlikely to be reassigned.

### Draft Field Split

This is a concrete starting point to evaluate:

```text
left S T K P W        starter code             5 bits, 32 states
left H R + A O * E U  grammar/state code       7 bits, 128 states
right F R P B L G     verb code                6 bits, 64 states
right T S D Z         suffix/object code       4 bits, 16 states
#                     reserved bank/extension  1 bit
```

Use right-hand names here in the steno sense:

```text
right F R P B L G T S D Z
```

This split is intentionally not Jeff's physical allocation. It preserves some useful Jeff-like ideas while creating stable room for suffixes:

- `A/O` can still participate in modal choices such as can/should/will.
- `*` can still participate in negation.
- `E` and left `R` can still participate in aspect/structure.
- left `R` can take over Jeff's `F` role for "have/perfect", because right `F` belongs to the verb field.
- right `T S D Z` become suffix/object codes, so right `D` is no longer inherently "past" in phrase mode.
- past tense moves into the grammar/state code.

The capacity of the no-number-bar layout is:

```text
32 starter states
128 grammar/state states
64 verb states
16 suffix states
```

That is enough to hold Jeff-scale grammar plus common one-stroke suffixes if the codebooks are designed carefully. `#` stays reserved for an extension bank rather than being taught in the first practice tier.

### Mnemonic Anchors Before Dense Fill

The phrase namespace is huge, but it should not feel like an arbitrary binary code.

Use a hybrid assignment rule:

1. Lock obvious mnemonic anchors first.
2. Reserve the easiest chords for the highest-frequency items.
3. Let the generator fill the remaining codebook by frequency and ergonomics.

The generator should accept fixed assignments such as:

```text
core verb B       -> be
core verb G       -> go
core verb BL      -> believe
core verb BG      -> become
core verb RPL     -> remember
core verb RPB     -> understand

core tail T       -> the
core tail D       -> it
core tail Z       -> that
core tail TS      -> this
core tail TZ      -> those

non-verb family T -> the/determiner phrases
non-verb family P -> prepositional phrases
non-verb family W -> wh/indefinite phrases
both-pedal S      -> style commands
```

These anchors intentionally copy the spirit of Jeff Phrasing without preserving every Jeff collision-avoidance constraint. For example, Jeff uses `B` for `to be`, `BL` for `to believe`, `BG`/`RPBG` for `become`, and `RPB` for `understand`. Stoin can keep those memorable anchors because pedal mode removes normal dictionary conflicts.

Starter anchors are harder because the draft split gives only `S T K P W` to the starter field. Still, use Jeff-like shadows where they fit:

```text
empty starter -> no subject; bare simple verbs emit infinitives such as "to be"
S             -> I
W             -> we
K             -> he
SK            -> she
P             -> it
T             -> they
ST            -> that
```

`you`, `this`, and `there` may need frequency-first assignments rather than perfect mnemonics. That is acceptable. The goal is not mnemonic purity; the goal is enough mnemonic scaffolding that the dense parts have something to hang from.

When an anchor conflicts with an abstract high-frequency assignment, prefer the anchor if the item is common enough to practice early. If an anchor would distort the whole table, demote it to a trainer hint instead of forcing it into the runtime map.

### Single Full Form

All phrase strokes use the same full-form interpretation:

```text
starter code       -> subject, empty subject, there, or demonstrative starter
grammar/state code -> tense, auxiliary, negation, aspect, inversion
verb code          -> verb
suffix code        -> object/article/preposition/tail
```

Example target:

```text
<starter he> + <past> + <verb see> + <suffix it> -> he saw it
```

Interrogatives and connectives should be modeled as future phrase families inside the same overall layout, not as a separate opener-plus-pronoun system. We should not add them until the layout contract says exactly how they compose without taking code space needed by subject starters.

### Grammar/State Bits In The Draft Split

The 7-bit grammar/state field decides how the starter and verb combine before the suffix is attached.

The cleanest core interpretation is a Cartesian grammar bank:

```text
H        past / conditional selector
A O      auxiliary family
*        negative
E left-R aspect
U        inversion / question order
```

Auxiliary family:

```text
no A/O -> no auxiliary in plain affirmative; do-support when negative or inverted
A      -> can / could
O      -> should, or shall / should if we want Jeff's wording
A+O    -> will / would
```

Aspect:

```text
no E/R -> simple finite verb
E      -> progressive: be + present participle
R      -> perfect: have + past participle
E+R    -> perfect progressive: have been + present participle
```

Examples:

```text
<he> + <see> + <it>                  -> he sees it
<he> + H + <see> + <it>              -> he saw it
<he> + * + <see> + <it>              -> he doesn't see it
<he> + H + * + <see> + <it>          -> he didn't see it
<he> + A + <see> + <it>              -> he can see it
<he> + A + H + <see> + <it>          -> he could see it
<he> + A + * + <see> + <it>          -> he can't see it
<he> + A + O + <see> + <it>          -> he will see it
<he> + A + O + H + <see> + <it>      -> he would see it
<he> + E + <see> + <it>              -> he is seeing it
<he> + R + <see> + <it>              -> he has seen it
<he> + E + R + <see> + <it>          -> he has been seeing it
<he> + U + <see> + <it>              -> does he see it
<he> + A + U + <see> + <it>          -> can he see it
```

This complete core bank uses all 128 grammar states:

```text
2 tense states * 4 auxiliary states * 2 polarity states * 4 aspect states * 2 order states = 128
```

That is attractive because every no-number-bar grammar chord has a predictable reading. The tradeoff is that modifiers like `just`, `still`, `never`, `even`, and per-stroke contraction overrides cannot also be independent axes inside the same 7 bits. They need to be output policy, extension-bank states, or deliberately selected replacements for some low-value core states.

This is a draft, not a decision. The important part is to decide this kind of grammar skeleton before any serious practice.

### Contractions

There is room for contractions if we treat them as output style rather than another independent grammar bit.

Negative contractions are already the natural output of the `*` grammar bit:

```text
do not / does not / did not -> don't / doesn't / didn't
cannot / could not          -> can't / couldn't
should not                  -> shouldn't
will not / would not        -> won't / wouldn't
```

Positive contractions should be a phrase-engine style setting at first:

```text
I am       -> I'm
you are    -> you're
he is      -> he's
I have     -> I've
they will  -> they'll
I would    -> I'd
```

A style setting costs no stroke space and avoids teaching duplicate strokes early. Later, if per-stroke control matters, use the reserved `#` extension bank for "force contracted" or "force long form." Do not spend one of the 4 suffix bits on contraction; suffix space is too valuable.

### Modifiers

Jeff-style modifiers such as `just`, `still`, `never`, `even`, and `always` should not be squeezed into the core grammar bank until we know which core states are truly worth keeping.

Options:

- Put modifiers in the reserved `#` extension bank.
- Replace low-value core states after the generator shows which states are rare or awkward.
- Treat some modifiers as suffix/tail codes when they naturally appear after the verb phrase.

The safest default is: core grammar first, contractions as style, modifiers in the both-pedal operator plan.

### One Pedal Versus Two Pedals

With the draft no-number-bar split, one phrase pedal already gives a large structured namespace:

```text
32 starters * 128 grammar states * 64 verbs * 16 suffixes = 4,194,304 combinations
```

Adding a second phrase pedal can be modeled as one extra top-level namespace bit:

```text
2 pedal namespaces * 4,194,304 = 8,388,608 combinations
```

If the app also treated pressing both phrase pedals as a third mode, the raw count would be:

```text
3 pedal namespaces * 4,194,304 = 12,582,912 combinations
```

The stronger argument for two pedals is not raw capacity. One pedal already has more theoretical capacity than we can comfortably memorize. The stronger argument is **separation of mental models** and respecting the ergonomic cost of a pedal press.

Do not spend a phrase-pedal stroke on a word that standard dictionaries already write well in one stroke. For example, `because` by itself is not a compelling second-pedal phrase if it is already easy in an existing standard dictionary. The second pedal should earn its keep by producing combinations that are awkward, multiword, or combinatorial.

Recommended two-pedal split:

```text
left phrase pedal    core finite-clause builder
right phrase pedal   non-verb phrase builder
both phrase pedals   interstitial core modifier/operator
```

Core finite-clause builder:

```text
starter + grammar/state + verb + suffix
he + past + see + it -> he saw it
```

Non-verb phrase builder:

```text
preposition + object/tail
subordinator + object/tail
quantifier/pronoun phrase
fixed multiword function phrase
```

Interstitial core modifier/operator:

```text
pending modifier for the next core phrase
retroactive modifier for the previous core phrase
style/contraction override for a core phrase
```

Two pedals are justified if the generator shows one of these problems:

- Common modifiers force us to replace useful core grammar states.
- Common suffixes exceed the 16-code suffix field.
- Interrogative/connective phrase families need a different grammar shape from subject-verb-object phrases.
- We want per-stroke contraction/style controls without spending suffix bits.
- The first pedal's codebook becomes too abstract to practice comfortably.

Two pedals are not justified merely because they double the raw namespace. Capacity is already large; stability and ergonomics are the real reasons.

### Second Pedal: Non-Verb Phrases

The second phrase pedal should focus on material that is not a finite verb phrase.

```text
core pedal        -> say a clause
non-verb pedal    -> say a phrase chunk
both pedals       -> modify a clause
```

Good second-pedal targets:

```text
with anybody else
with anything
as to whether
in order to
rather than
one of the
some of the
any of them
all of this
for the first time
on the other hand
```

Weak second-pedal targets:

```text
because
however
therefore
what
where
```

Those may still appear inside longer second-pedal phrases, but they should not be taught as the reason to use the pedal if ordinary dictionary theory already handles them well.

Use the same physical field split as the core pedal, but reinterpret the fields for non-verb phrases:

```text
left S T K P W        phrase family            5 bits, 32 families
left H R + A O * E U  determiner/state         7 bits, 128 states
right F R P B L G     phrase head              6 bits, 64 heads per family
right T S D Z         tail/object code         4 bits, 16 common tails
#                     rare/extended bank       1 bit
```

The first practice tier should use only the easy single-key family codes. Multi-key family codes stay reserved.

```text
no S/T/K/P/W  pronoun/quantifier chunk
S             some/same/quantity chunk
T             the/determiner chunk
K             complementizer/subordinator phrase
P             prepositional phrase
W             wh/indefinite phrase
ST            fixed function phrase, second practice tier
```

The non-verb state bits should be less ambitious than the core grammar bits. A draft interpretation:

```text
H        plural / collective variant
R        reflexive / reciprocal / "else" variant
A O      determiner or quantifier sub-bank
*        negative/exclusion variant: no, not, except, without
E        "else" / additionality variant
U        interrogative/indefinite variant
```

Do not freeze that state map until a generator has produced phrase lists. The important design rule is that the second pedal composes phrase chunks, not clauses.

#### Shared Tail/Object Codes

The right `T S D Z` tail field should be shared by the core and non-verb pedals. A starter table:

```text
empty   no tail
T       the
S       a/an
D       it
Z       that
TS      this
TD      me
TZ      those
SD      her
SZ      us
DZ      them
TSD     you
TSZ     these
TDZ     him
SDZ     one
TSDZ    all
```

For the core pedal, this gives direct objects and article tails:

```text
he saw it
it is a/an
she believed her
```

For the non-verb pedal, the same tail field combines with prepositions, quantifiers, and fixed phrase heads:

```text
to me
for you
with them
about it
one of the
any of them
all of this
as to whether
because of that
```

`S` should mean an automatic indefinite article. The phrase engine can choose `a` or `an` from the next word when the next word is known, and default to `a` otherwise.

#### Non-Verb Item Tables

The right-hand `F R P B L G` phrase head is a per-family codebook. The exact assignments should be generated by frequency and ergonomics, but the first tier can start with tables like these.

Pronoun/quantifier chunk, no left family key:

```text
F       anybody
R       anyone
P       anything
B       everybody
L       everyone
G       everything
FR      nobody
FP      nothing
FB      somebody
FL      someone
FG      something
RP      each
RB      either
RL      neither
RG      both
PB      all
```

Some/same/quantity chunk, left `S`:

```text
empty   some
F       some of
R       same
P       several
B       such
L       little
G       single
FR      most of
FP      many of
FB      a lot of
FL      the rest of
FG      the other
RP      another
RB      the same
RL      the first
RG      the last
PB      the next
```

The/determiner chunk, left `T`:

```text
empty   the
F       one of
R       some of
P       any of
B       all of
L       each of
G       both of
FR      the first
FP      the next
FB      the last
FL      the rest of
FG      the other
RP      the same
RB      the whole
RL      the only
RG      the best
PB      the most
```

Fixed function phrase, left `ST`, second practice tier:

```text
F       in order to
R       so as to
P       rather than
B       other than
L       instead of
G       in spite of
FR      as well as
FP      as soon as
FB      as long as
FL      on the other hand
FG      for the first time
RP      at the same time
RB      by the way
RL      in other words
RG      as a result
PB      for example
```

Complementizer/subordinator phrase, left `K`:

```text
F       whether
R       that
P       if
B       when
L       where
G       why
FR      how
FP      what
FB      because of
FL      in case
FG      except that
RP      even though
RB      as though
RL      provided that
RG      assuming that
PB      given that
```

Preposition family, left `P`:

```text
empty   to
F       of
R       for
P       in
B       on
L       with
G       from
FR      about
FP      into
FB      over
FL      under
FG      between
RP      through
RB      during
RL      without
RG      against
PB      as to
```

Wh/indefinite phrase, left `W`:

```text
empty   what
F       who
R       where
P       when
B       why
L       how
G       which
FR      whose
FP      whatever
FB      whoever
FL      wherever
FG      whenever
RP      however
RB      what else
RL      who else
RG      where else
PB      how else
```

This keeps the second pedal focused on phrases that are harder to get from a regular dictionary stroke:

```text
with anybody else
with anything
as to whether
rather than that
one of them
some of the
whatever else
in order to
```

### Both Pedals: Interstitial Core Operators

Modifiers like `just`, `still`, `already`, `even`, `only`, and `probably` are usually valuable because they sit inside a core phrase:

```text
he just saw it
they still didn't go
she probably would have believed her
```

That is different from simply prefixing or appending another word. These belong in a third namespace selected by holding both phrase pedals.

Start with both-pedal strokes as operator strokes, not as complete modified-core strokes. That means the operator is a separate stroke that modifies the next or previous core phrase:

```text
BOTH[pending just]
CORE[he past see it]
=> he just saw it

CORE[he past see it]
BOTH[retro just]
=> he just saw it

BOTH[pending force-contracted]
CORE[they will go]
=> they'll go

BOTH[pending probably]
CORE[she conditional perfect believe her]
=> she probably would have believed her
```

This costs an extra stroke, but it avoids stealing bits from the core phrase. A future one-stroke modified-core namespace is possible, but it would need a real field tradeoff. For example, it might reduce verb or suffix capacity to make room for a modifier axis. Do not choose that until practice data shows which modifiers are worth one-stroke treatment.

Suggested both-pedal field split:

```text
left S T K P W        operator family          5 bits, 32 families
left H R + A O * E U  scope/placement/style    7 bits, 128 states
right F R P B L G     operator item            6 bits, 64 items per family
right T S D Z         optional object/tail      4 bits, 16 tails
#                     rare/extended bank       1 bit
```

Scope/placement draft:

```text
H        retroactive: apply to previous core phrase
R        pending: apply to next core phrase
A/O      placement: before auxiliary, after auxiliary, before verb, phrase-final
*        contrastive/negative form where meaningful
E        force contracted/style variant
U        question/order variant where meaningful
```

If neither `H` nor `R` is present, default to pending. This avoids teaching a scope bit for the common "operator before phrase" pattern.

Both-pedal operator families:

```text
no S/T/K/P/W  interstitial adverb/modifier
S             style/contraction/casing
T             punctuation/glue/phrase join
K             modal attitude/evidential
P             retro/preposition attachment to previous phrase
W             question/relative reshaping
```

Interstitial modifier items, no left family key:

```text
F       just
R       still
P       already
B       even
L       only
G       also
FR      really
FP      probably
FB      maybe
FL      almost
FG      always
RP      never
RB      usually
RL      actually
RG      again
PB      now
```

Style/control items, left `S`:

```text
F       force contracted
R       force long form
P       title-case phrase
B       lower-case phrase
L       capitalize phrase
G       no-space/glue phrase
FR      hyphenate phrase join
FP      quote phrase
FB      parenthesize phrase
FL      comma before phrase
FG      comma after phrase
RP      reset phrase style
```

The style family generates commands rather than ordinary text. These commands should still become translation-history entries so undo can restore the previous style state.

#### What Each Pedal Owns

The core pedal owns grammar that is part of a finite predicate:

```text
subject
tense
auxiliary
negation
aspect
verb
direct object or very common tail
```

The non-verb pedal owns phrase material that is not the central predicate:

```text
pronoun/quantifier chunks
determiner/noun chunks
fixed multiword function phrases
complementizer/subordinator phrases
prepositional phrases
rare tail/object overflow
```

Both pedals together own operators that affect a core phrase:

```text
interstitial adverbs/modifiers
style and contraction overrides
retroactive phrase operators
pending phrase operators
punctuation/glue around a phrase
```

This gives the two pedals a teachable distinction:

```text
left foot       -> say the clause
right foot      -> say the non-verb phrase chunk
both feet       -> modify the clause
```

Do not move common core phrases or easy one-word dictionary entries to the non-verb pedal just because there is space. The second pedal should protect the core map from churn, not become a junk drawer.

### Layout Principles

- Use mnemonic strokes where they are obvious and cheap.
- Prefer dense abstract codes where mnemonic mapping would waste large parts of the keyspace.
- Assign the easiest chords to the most common starters, verbs, and suffixes.
- Keep the abstract code tables stable once practice begins.
- Grow in tiers so practice stays incremental.
- Generate layout validation reports instead of reasoning about every possible chord by hand.

### Candidate Phrase Fields

Starter examples for the left-hand codebook:

```text
I, you, he, she, it, we, they,
this, that, there-singular, there-plural,
empty-singular, empty-plural
```

Reserved future starter families:

```text
what, when, where, why, who, how,
if, for, but, and
```

Verb examples for the right-hand codebook:

```text
be, do, go, see, say, believe, think, know, want, need,
have, make, get, take, come, give, find, tell, ask, use
```

Suffix examples that should be considered first-class, not only an afterthought:

```text
a, an, the,
it, me, you, him, her, us, them,
that, this,
to, of, for, in, on, with, from, about
```

This is enough to target:

```text
was a
was it
it is a
he saw it
he believed her
they didn't go
```

without needing a second suffix stroke for the common cases.

## Pedal Mode Semantics

A pedal role is not a steno key; it is an out-of-band stroke modifier. Phrase pedals select a phrase namespace.

Phrase mode should mean:

- If the core phrase pedal is held for a stroke, the completed stroke is interpreted by the core finite-clause phrase engine.
- If the non-verb phrase pedal is held for a stroke, the completed stroke is interpreted by the non-verb phrase engine.
- If both phrase pedals are held for a stroke, the completed stroke is interpreted by the interstitial core-operator engine.
- If no phrase pedal is held, behavior is unchanged.
- Phrase mode checks the phrase engine first.
- A phrase miss should fall back to the regular dictionary stack. If the normal stack also misses, it emits the raw chord just like ordinary untranslated steno.
- Stroke tracing should mark phrase-engine hits as `[phrase]` and phrase misses that use the regular stack as `[phrase fallback]`.

For qwerty chord gathering, "during the stroke" can be exact:

- Track current pedal roles in `Steno`.
- When any steno key goes down, copy currently-held pedal roles into the in-progress chord.
- If a phrase pedal goes down while qwerty steno keys are already held, mark the in-progress chord with that phrase namespace.
- When all qwerty steno keys are released, finish the chord with the accumulated flags.

For TX Bolt, Gemini PR, and multi-machine serial input, the machine sends only completed strokes. There is no per-key press/release stream for Stoin to observe. For serial inputs, phrase namespace is sampled from current pedal state plus a one-stroke latch. Pressing a phrase pedal marks the next completed serial stroke with that namespace, even if the pedal is released before the serial packet is processed.

## Platform Pedal Layer

Revive the stashed macOS pedal work as reference, not as a direct patch. The stash used the right general shape:

```c
typedef enum Platform_Pedal_Role {
    PLATFORM_PEDAL_ROLE_NONE,
    PLATFORM_PEDAL_ROLE_PHRASE_CORE,
    PLATFORM_PEDAL_ROLE_PHRASE_NONVERB,
    PLATFORM_PEDAL_ROLE_COUNT,
} Platform_Pedal_Role;

typedef void (*Platform_Pedal_Event_Fn)(
    Platform_Pedal_Role role,
    bool is_down,
    void *userdata
);
```

The implemented API is:

```c
bool platform_pedals_init(
    const char *config_path,
    Platform_Pedal_Role register_role,
    Platform_Pedal_Event_Fn callback,
    void *userdata
);
void platform_pedals_poll(void);
void platform_pedals_shutdown(void);
```

`platform_pedals_poll()` is still useful on macOS. Qwerty mode naturally runs the CFRunLoop, but serial modes need to pump pending IOHID callbacks from the serial polling loop before each completed stroke is translated.

### Registration

Add:

```text
--register-pedal core|nonverb
--pedal-config PATH
```

`--register-pedals` is accepted as a compatibility alias for registering the core phrase pedal.

Default path:

```text
stoin-pedals.json
```

Registration flow:

1. Start Stoin with `--register-pedal core` or `--register-pedal nonverb`.
2. Prompt for the chosen role, for example "press the pedal to use for core phrase mode".
3. Platform captures device identity and button identity.
4. Persist role mapping.
5. Future runs silently load that mapping.

Implemented macOS config shape:

```json
{
  "version": 1,
  "phrase_core": {
    "vendor_id": 1234,
    "product_id": 5678,
    "location_id": 9012,
    "usage_page": 9,
    "usage": 1
  },
  "phrase_nonverb": {
    "vendor_id": 1234,
    "product_id": 5678,
    "location_id": 9012,
    "usage_page": 9,
    "usage": 2
  }
}
```

Linux can use the same role names with Linux-specific fields:

```json
{
  "version": 1,
  "phrase_core": {
    "device_name": "USB Foot Switch",
    "physical_path": "usb-0000:00:14.0-3/input0",
    "event_code": 256
  }
}
```

### macOS

Use IOHIDManager in `src/platform_macos_pedals.c`.

Current approach:

- Enumerate HID devices that expose button-like elements.
- On registration, listen for the next keyboard/button-page down event and persist device + element identity.
- On normal startup, open only configured devices.
- Try to open configured keyboard-page devices exclusively so pedal key events do not leak into the active app.
- Treat normal text-producing keyboard usages as unsafe pedal triggers. A downstream CGEvent tap cannot reliably distinguish a pedal's `a` from a real keyboard `a`, and even an apparent exclusive IOHID open might not stop macOS from typing it.
- Allow keyboard-page pedals mapped to F13-F24, because those are non-printing keys and can run in shared mode if exclusive open is unavailable.
- Emit `Platform_Pedal_Event_Fn(role, is_down, userdata)`.
- Call `platform_pedals_shutdown()` from `platform_shutdown()`.

### Linux

Linux can use evdev under `/dev/input/event*`.

Expected approach:

- Open configured event devices read-only.
- Optionally grab the configured pedal device with `EVIOCGRAB` so the pedal does not leak a keypress to the active app.
- Poll events in `platform_pedals_poll()`.
- Convert matching `EV_KEY` codes to pedal role events.
- Reuse the existing Linux input permissions story from `docs/linux-setup.md`: users need read access to the configured `/dev/input/event*`.

For discovery/registration, inspect devices with:

- `EVIOCGNAME`
- `EVIOCGPHYS`
- `EVIOCGBIT(EV_KEY, ...)`

Pedals often look like tiny keyboards or joystick/button devices, so registration should trust the user's "press this pedal now" signal more than hardcoded device classes.

## Steno Core Changes

The steno core should move from "stroke bits only" to "stroke bits plus phrase namespace".

Suggested type:

```c
typedef enum Phrase_Namespace {
    PHRASE_NAMESPACE_NONE,
    PHRASE_NAMESPACE_CORE,
    PHRASE_NAMESPACE_NONVERB,
    PHRASE_NAMESPACE_CORE_OPERATOR,
} Phrase_Namespace;

typedef struct Stroke_Input {
    uint64_t bits;
    Phrase_Namespace phrase_namespace;
    uint64_t received_ns;
} Stroke_Input;
```

Add:

```c
bool steno_handle_stroke(Steno *steno, Stroke_Input stroke);
void steno_set_phrase_namespace(Steno *steno, Phrase_Namespace namespace, bool is_down);
```

Then keep compatibility wrappers:

```c
bool steno_handle_stroke_bits(Steno *steno, uint64_t bits);
```

Qwerty capture uses `steno_set_phrase_namespace()` to accumulate phrase namespace during chord gathering. Serial input calls `steno_handle_stroke()` directly with the namespace sampled from current pedal state.

The steno layer should derive the namespace from currently-held pedal roles:

```text
no phrase pedal       -> PHRASE_NAMESPACE_NONE
core pedal            -> PHRASE_NAMESPACE_CORE
non-verb pedal        -> PHRASE_NAMESPACE_NONVERB
both phrase pedals    -> PHRASE_NAMESPACE_CORE_OPERATOR
```

## Phrase Engine Shape

Implement the first phrase engine in C, probably:

```text
src/phrasing.h
src/phrasing.c
```

Proposed API:

```c
typedef enum Phrase_Lookup_Result {
    PHRASE_LOOKUP_MISS,
    PHRASE_LOOKUP_HIT,
    PHRASE_LOOKUP_ERROR,
} Phrase_Lookup_Result;

Phrase_Lookup_Result phrasing_lookup(
    Phrase_Namespace namespace,
    uint64_t stroke_bits,
    char **out_utf8
);
```

The phrase engine should not know about platform code, dictionaries, output, or undo. It should be a pure lookup/generation module.

In `steno.c`:

```text
completed stroke
  if phrase namespace:
    phrase lookup in that namespace
      hit  -> apply generated text as a normal translation
      miss -> fall back to regular dictionary stack
  else:
    existing dictionary translation path
```

Phrase translations should create normal `Translation` entries so:

- `=undo` works.
- Retroactive replacement remains possible later.
- Translation timing measures the same end-to-end path.
- Stroke tracing can print `<phrase chord> -> he saw it`.

## Why Not Lua First

Lua is worth keeping in mind, but this is probably not the right first Lua plugin.

Reasons to start in C:

- The phrase parser needs direct bitset/stroke-order logic.
- We will want dense tests around grammar and layout validity.
- The plugin boundary is not designed yet.
- Adding Lua now creates build/package decisions before we know the stable host API.
- A pure C phrase engine can later become the reference implementation for a Lua-facing `lookup_phrase(bits, flags)` API.

Good Lua follow-up shape:

```lua
function lookup(stroke)
  -- stroke.bits, stroke.outline, stroke.flags.phrase_mode
  return "he saw it" -- or nil
end
```

But the host API should come after the C phrasing engine proves what data plugins need.

## Incremental Grammar Plan

### Phase 1: Dense Layout Generator

Write a small generator/layout validator before committing to a runtime phrase table or any practice deck.

Input tables:

```text
starter candidates ranked by usefulness
verb candidates ranked by usefulness
grammar/structure candidates
suffix candidates ranked by usefulness
available steno bit fields from the layout contract
ergonomic weights for each chord
reserved/future banks
```

Output:

- proposed starter codebook,
- proposed grammar/state codebook,
- proposed verb codebook,
- proposed suffix codebook,
- accepted one-stroke phrase entries,
- unused capacity,
- rejected duplicate or invalid internal assignments,
- ergonomic score report,
- practice tier assignments.

The key point: phrase mode removes normal dictionary collisions. The validator is only for our own layout bookkeeping: duplicate fixed anchors, generated entries that claim the same phrase-mode chord, unreachable states, reserved-bank leaks, and awkward assignments we may want to review before making them muscle memory.

### Phase 2: Stoin-Native Core

Build the phrase engine around a Stoin-native layout, not Jeff's exact strokes.

Use Jeff's code as the grammar/conjugation reference:

- subject agreement,
- present vs past,
- negation,
- `do` omission rules,
- `have` / `be` structures,
- participle selection,
- verb-specific forms.

But define our own tables:

```text
Starter_Code -> starter, grammatical person/form
Verb_Code    -> verb forms
Grammar_Code -> tense, auxiliary, negation, structure
Suffix_Code  -> article/object/preposition/tail
```

The first target set should prove the model with one-stroke phrases:

```text
phrase(<it> + <be present> + <a>)       -> it is a
phrase(<he> + <see past> + <it>)        -> he saw it
phrase(<he> + <believe past> + <her>)   -> he believed her
phrase(<they> + <go past negative>)     -> they didn't go
phrase(<empty-singular> + <be past> + <a>)  -> was a
phrase(<empty-singular> + <be past> + <it>) -> was it
```

The tests should assert generated English first. Stroke spellings can change while we are still designing the table, but once practice starts the mapping should become stable.

### Phase 3: Suffix Expansion

Common suffixes should be part of the one-stroke design immediately, not treated as rare extensions.

Candidate suffix categories:

```text
articles:        a, an, the
object pronouns: it, me, you, him, her, us, them
demonstratives:  this, that
prepositions:    to, of, for, in, on, with, from, about
connective tails: that, whether, because
```

Second phrase-mode suffix strokes are still useful, but as overflow:

```text
hold phrase pedal, stroke <he saw>  -> he saw
hold phrase pedal, stroke <it>      -> he saw it
```

That fallback gives us room for less common endings without forcing every suffix into the first one-stroke layout.

### Phase 4: More Jeff Features

Once the core works:

- empty starters / infinitives,
- `there` restrictions,
- `always`,
- `just`, `still`, `never`, `even`,
- auxiliary-only verbs like `may`, `must`, `shall`, `will`,
- reverse lookup or SRS deck generation,
- optional contractions.

## Practice Strategy

Borrow the preset idea from `~/code/phrasing-trainer`.

Initial practice decks:

1. Basic full starters:
   `I`, `you`, `he`, `she`, `it`, `we`, `they`.
2. Basic verbs:
   `be`, `go`, `see`, `say`, `do`, `believe`, `think`, `know`.
3. Past toggle:
   present vs past.
4. Negation:
   `don't`, `doesn't`, `didn't`.
5. One-stroke suffixes:
   `a`, `it`, `you`, `her`, `them`.
6. Structures:
   `have`, `be -ing`, `have been -ing`.

This can feed the existing Stoin SRS web code later: generate prompt/answer pairs from the same phrase engine tables rather than maintaining a separate trainer grammar.

## Testing Plan

Add pure phrase-engine tests first:

- decode Stoin-native starter, verb, grammar, and suffix fields,
- generate the target starter/verb/suffix phrases,
- handle present/past forms,
- handle subject agreement,
- handle negation and `do` omission,
- report duplicate, reserved, or unreachable layout assignments from the generator.

Add steno integration tests:

- normal stroke without phrase namespace still uses dictionary,
- same stroke with core phrase namespace uses the core phrase engine,
- same stroke with non-verb phrase namespace uses the non-verb phrase engine,
- same stroke with both-pedal namespace uses the core-operator phrase engine,
- phrase miss falls back to the regular dictionary stack,
- phrase translation can be undone,
- phrase output respects leading spacing policy,
- qwerty pedal-down during chord sets phrase namespace,
- serial stroke samples current pedal phrase namespace.

Add platform tests where feasible by keeping platform pedal parsing split from OS event delivery.

## Open Questions

- What exact WSI mapping do we want for starter, verb, grammar, and suffix fields?
- Phrase misses fall back to the regular dictionary stack.
- Should phrase-mode suffix strokes be allowed after normal dictionary words, or only after phrase outputs?
- Do we want to ship with only `phrase_core`, or register `phrase_nonverb` from the start?
- Do we still want non-phrase pedal roles such as `number` and `star`?
- Should pedal registration live in `stoin-config.json`, `stoin-pedals.json`, or both?
- How abstract are we willing to make the starter and verb tables in exchange for capacity?
- How much of Jeff's reverse lookup do we care about before SRS generation?

## Recommended First Implementation

1. Freeze a draft field split, starting with the one in this document unless we find a better one.
2. Write the standalone layout generator/validator.
3. Generate and review starter, grammar/state, verb, and suffix codebooks.
4. Add a small C phrase engine with that Stoin-native table subset:
   - starters: `I`, `you`, `he`, `she`, `it`, `we`, `they`,
   - verbs: `be`, `go`, `see`, `believe`, `do`,
   - suffixes: `a`, `it`, `her`,
   - negation and past tense.
5. Add tests for the examples in this doc.
6. Add `Stroke_Input` and phrase namespaces to the steno core.
7. Add platform-neutral pedal roles and a macOS IOHID pedal module.
8. Route qwerty and serial strokes through the new stroke-input path.
9. Expand starter, verb, and suffix tables in practice tiers.
10. Add overflow phrase-mode suffix strokes for rare endings.
11. Add Linux evdev pedal support.
12. Expand grammar toward full Jeff compatibility.

That gets us useful phrases quickly while preserving the path toward a much more expressive system.
