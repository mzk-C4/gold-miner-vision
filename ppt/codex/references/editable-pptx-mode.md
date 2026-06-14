# Editable PPTX Mode

Use this reference when the user asks for editable PowerPoint output, selectable text, movable objects, or "图片生成后再转可编辑 PPT".

## Mode Contract

Editable mode has two stages:

1. Generate and QA the complete slide images exactly as normal Codex PPT.
2. Reconstruct each approved slide image into editable PowerPoint layers, then assemble one final `.pptx`.

Keep the approved slide images unchanged. Put editable reconstruction artifacts in a separate `editable/` folder so image-mode evidence and editable-mode derived artifacts do not overwrite each other.

## Recommended Folder Layout

```text
deck-name/
├── brief.md
├── storyline.md
├── design-system.md
├── storyboard.md
├── prompts/
├── slides/
├── qa/
├── editable/
│   ├── subagents/
│   │   ├── slide-01/
│   │   ├── slide-02/
│   │   └── ...
│   ├── visual-layers/
│   ├── text-layer.json
│   ├── workspace/
│   ├── preview/
│   ├── layout/
│   └── qa/
└── final/
```

## Per-Slide Subagent Contract

Launch one subagent per slide when a subagent capability is available. If not available, process the same contract sequentially.

Give each subagent only the inputs for its slide:

- approved slide image, such as `slides/slide-03-topic.png`
- that slide's storyboard entry
- relevant design-system excerpt
- exact approved on-slide text from the storyboard or QA notes
- target slide size, usually `1920x1080` for image generation and `960x540` or matching source pixels for editable coordinates

Each subagent must produce:

```text
editable/subagents/slide-XX/
├── source.png
├── visual-layers/
│   └── slide-XX/
│       ├── manifest.json
│       └── *.png
├── text-layer.json
└── qa/qa.md
```

The `manifest.json` records textless visual assets with positions. `text-layer.json` records native PowerPoint text objects. Do not leave final body text baked into the visual layer unless explicitly documenting an accepted hybrid route.

Treat the approved text from storyboard/QA as the content source of truth. AI-rendered text inside the source image is layout evidence only when it conflicts with the approved text.

## Reconstruction Routes

Prefer routes in this order:

1. `component-layer route`: split cards, rules, diagrams, icons, and backgrounds into transparent PNG layers; rebuild text as editable text boxes.
2. `textless-skeleton route`: use a full-slide textless skeleton image when atomic splitting is too slow or visual geometry is too complex.
3. `hybrid route`: textless skeleton plus selected component repairs for broken labels, badges, icons, or chart frames.

Complex charts, photos, logos, dense illustrations, and screenshots may remain image objects. Titles, bullets, labels, captions, page numbers, and callouts should be editable text.

## Merge Per-Slide Outputs

After subagents finish, merge their outputs:

```bash
python /path/to/codex-ppt/scripts/merge_editable_slide_outputs.py \
  /path/to/deck-name/editable/subagents \
  --out-layers-root /path/to/deck-name/editable/visual-layers \
  --out-text-json /path/to/deck-name/editable/text-layer.json \
  --force
```

Then build the editable PPTX:

```bash
node /path/to/codex-ppt/scripts/build_editable_ppt_from_layers.mjs \
  --backend auto \
  --layers-root /path/to/deck-name/editable/visual-layers \
  --text-json /path/to/deck-name/editable/text-layer.json \
  --out /path/to/deck-name/final/deck.editable.pptx \
  --workspace /path/to/deck-name/editable/workspace \
  --preview-dir /path/to/deck-name/editable/preview \
  --layout-dir /path/to/deck-name/editable/layout \
  --slide-size 960x540
```

Run `npm install` in the `codex-ppt` skill folder if the fallback `pptxgenjs` backend is needed. In Codex environments with the Presentations artifact runtime, `--backend auto` can also emit preview PNGs and layout JSON.

## QA Gates

Before delivery:

- PPTX opens as a zip and has the expected slide count.
- Every slide has semantic text in `ppt/slides/slide*.xml`.
- Visual assets are separate selectable objects, not only one final screenshot, unless the route is explicitly accepted as textless skeleton or hybrid.
- No duplicate text appears from residual baked source text plus editable overlay.
- Text boxes are transparent and do not use fills or outlines to patch shapes.
- Page order and page numbers match the storyboard.
- Compare the final editable PPTX against approved slide images and document visible differences in `editable/qa/qa.md`.
