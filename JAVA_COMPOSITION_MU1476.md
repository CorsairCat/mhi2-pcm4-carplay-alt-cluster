# PCM4 MU1476 Java composition and displayable candidates

Read-only investigation, 2026-10-25. Input JXE SHA-256:
`1eccb000da3ef57e35dec0d59e4b0be5df375b20b41457fcd53f9bca75307a88`.
ROM SHA-256: `54de7f366f29be2bc2624d3b9d35f781c29f1dfa4ca707d1bf506c1aa4177ea7`.
Addresses below are ROM method offsets; PC values are local bytecode positions,
not deployable process offsets. No vendor code executed or modified.

## What Java actually submits

`DisplayManagerMIB2PorscheHigh.defineContexts`, ROM `0x1d4c8b0`, constructs 76
literal DisplayContext objects. Relevant entries are:

| Context | Displayables | Interpretation from native names |
| --- | --- | --- |
| 72 | 33 | Kombi map |
| 73 | 22 | 3D intersection |
| 74 | 22, 33 | Intersection and Kombi map |
| 75 | empty | No displayable listed; not proof the whole panel is blank |
| 76 | 58 | Google Earth Kombi map |
| 77 | 22, 58 | Intersection and Google Earth Kombi map |
| 78 | 16, 59 | HMI and external smartphone |

The main map has context 10 [16,19] and richer compositions such as context12
[16,20,22,19]. Thus logical displayables can occur in several compositions;
claiming an existing displayable can affect more than the cluster page.
Context54 contains [23,76], so the native debug-name table ending at60 is not
a universal displayable-ID range limit.

Base `DisplayManager.initDSI`, ROM `0x1d385f8`, calls virtual defineContexts at
PC15, then DSIDisplayManagement.declareContexts(DisplayContext[]) at PC26,
configureDM at PC32, and sets initialized=true. The static context lists are
therefore handed to the native service, not merely unused Java constants.

Porsche switchContext, ROM `0x1d4d380`, optionally adjusts the context through
addKDKToContext before its base call at PC42. Base switchContext, ROM
`0x1d386b4`, preserves the listener, maps terminal/context and assigns a request
session ID. It invokes DSIDisplayManagement.switchContext(III) at PC178, with a
retry at PC386. Java terminal1 maps to internal display4. This is routing/control,
not posting each pixel frame; the selected producers supply their own surfaces.

## Navigation application's selection path

The additional ROM inspection decoded 101 map/context classes (1848 method
bodies, zero class errors), then seven service/mapping classes (421 method bodies,
zero errors). Reports stay ignored under private/analysis/mu1476-display-port:
`hmi-composition-trace.json` and `hmi-composition-services.json`.

`Context.switchDisplayContextKombi`, ROM `0x21d5ee8`:

1. Reads the active satellite-map renderer ID.
2. Calls MapUtils.contextToHMIServiceID with the requested map-context enum,
   renderer ID, terminal1 and navigation environment (PC32).
3. Gets choice model (1,168) at PC44.
4. If the requested service differs, sets that model's value at PC122 while
   holding the route-info monitor.

`MapUtils.contextToHMIServiceID`, ROM `0x22db52c`, resolves these switch cases:

| Map-context enum | Returned display/HMI service context |
| --- | --- |
| 8 | renderer ID3 ->72; other renderer ->76 |
| 9 | 73 |
| 10 | renderer ID3 ->74; other renderer ->77 |
| 11 | 55 |
| 12 | renderer ID3 ->53; other renderer ->54 |
| Default with terminal1 | renderer ID3 ->72; other renderer ->76 |

Case8 begins PC180, case9 PC197, case10 PC211, case11 PC204 and case12 PC228.
The names/meaning of renderer ID3 are not assumed from its number. The
intermediate choice-model event dispatcher into DisplayManager is not fully
traced here; do not portray this as a complete event-by-event call stack.
The mapping and the native declarations independently agree on the stock
72/76 versus74/77 compositions.

## ID60: candidate, not known invalid

The native name table calls60 HUD_MAP_VIEW; none of the reconstructed Porsche
literal context lists contains it. This supports testing Audi's separate-surface
method and gives no evidence that60 is currently selected by these Java lists.
It does not establish absence of native/dynamic owners, duplicate stream-ID
semantics, priority behavior or physical routing. A common firmware enum can
include functions unused on a particular car. The name alone does not prove an
active HUD or a collision on this Macan.

| Candidate | Benefit | Unresolved issue / cost |
| --- | --- | --- |
| 60 | Matches Audi's working separate-displayable approach; absent from Porsche's literal contexts | Factory HUD name; native/dynamic ownership and stream behavior not established |
| Newly allocated ID, e.g.61 as an investigation example | Could keep map and main CarPlay producers separate | Need native accepted range, registration rules and a genuine unused allocation;61 is not selected |
| 33 | Proven normal Kombi-map membership | Already owned by stock map; sharing/replacing it needs explicit producer arbitration and complicates restoration |
| 58 | Proven alternate Kombi-map membership | Owned alternate-map semantics; configuration-dependent, not established spare |
| 59 | Proven external-smartphone membership in context78 | Risks interaction with the main CarPlay surface; not evidence of a separately usable auxiliary surface |

Unused-by-table video enums (e.g.29/30) are not automatically spare either;
feature-specific native producers may claim them. There is no verified list of
free IDs yet. Base Java allocates 110 opacity/position entries per terminal;
that proves the Java bookkeeping size, not native registration acceptance or a
pool of110 free IDs.

Preferred approach remains a separate displayable/context, with60 as the first
compatibility candidate and a genuinely allocatable new ID as the alternative.
Reusing33 would be a different ownership design, not a simpler equivalent to
Audi's approach. No candidate is promoted into the runnable profile here.

## Stock smartphone context78: additional Java evidence

A subsequent read-only pass inspected686 classes under de/audi/app/terminalmode,
3862 decoded method bodies with zero class errors. The report is ignored at
private/analysis/mu1476-display-port/hmi-terminalmode.json.

ExternalEventsListener.init, ROM0x15fab54, calls IContext.getChoiceModel with
integer3200035 at PC67, then sets that model to78 at PC74. This establishes
an actual terminal-mode initialization path selecting78 as its display choice,
beyond the DisplayContext declaration alone. The choice-model-to-display-manager
event dispatch and exact pixel placement/blending have not been fully traced.

Context78's [16,59] means the selected composition includes HMI and external
smartphone producers. It does not itself create an auxiliary stream, clone
CarPlay to another output, establish alpha/z-order, or prove what area the HMI
producer covers. Stock CarPlay video/session control supplies the smartphone
surface; the Java context selects membership. Copying78 to the cluster would
not request a separate navigation presentation from the phone.

## Implementation follow-up — 2026-10-25

The analysis above records the evidence and open questions at discovery time.
The offline integration candidate now implements guarded session hooks, a
separate renderer and readiness publisher, context900=[60] declaration, Java
handover, and private packaging/rollback. See
[the current implementation and validation record](PCM4_MU1476_DISPLAY_PORT.md)
and [the test procedure](integration/README.md). These implementations do not
convert conditional firmware findings into vehicle runtime proof.
