# Slide Quality Checklist

Use this checklist after each image generation pass and again before PDF compilation.

## Preflight

- All expected slides exist and match the storyboard count.
- Filenames sort in the intended order, such as `slide-01-cover.png`, `slide-02-agenda.png`.
- Every page is landscape 16:9. Prefer 1920x1080 or higher.
- Every final page is a full-slide AI-generated image, not an AI background with locally added text or shapes.
- Slide content stays inside a safe area; no title, label, logo, or page number touches the edge.
- The cover and closing slide feel intentionally designed, not like generic placeholders.

## Story and Content

- The deck has a visible chain: hook, context, tension, insight, evidence, conclusion, action.
- Each slide has one main takeaway.
- The title states the point, not just the topic.
- Body copy is concise enough to read in a presentation setting.
- Data, claims, and examples match the user's source material.
- No slide introduces an unexplained term, random statistic, or unsupported claim.

## Visual System

- Palette, type scale, margins, grid, icon style, and image treatment are consistent.
- Layout variants repeat intentionally instead of changing randomly.
- Visual density fits the audience and purpose.
- Important information is not buried under decorative images.
- Charts and diagrams have clear labels, enough contrast, and no fake precision.

## AI Image Issues

Regenerate the full slide when you see:

- misspelled, nonsensical, or warped text
- random symbols, fake logos, or stray marks
- distorted UI, charts, hands, faces, buildings, or products
- inconsistent style compared with the rest of the deck
- blur, low contrast, tiny text, or unreadable labels
- crowded composition with no clear focal point
- important content cropped near the edges

## Text Handling

- All visible text in the final slide image must be AI-rendered as part of the complete page.
- Do not add or repair text with HTML/CSS, canvas, Pillow, PowerPoint, Keynote, SVG, or any deterministic renderer.
- If exact text matters, keep it short and regenerate until it is visibly correct.
- Keep titles short and legible.
- Preserve the user's terminology exactly.
- Check Chinese punctuation, line breaks, and mixed Chinese/English spacing.

## Regeneration Rules

- Regenerate when the composition, style, or image quality is wrong.
- Regenerate when text placement, alignment, or a small label is wrong; do not locally repair or recompose it.
- Rewrite the prompt before regenerating; do not repeat a failed vague prompt.
- Tighten the prompt with the exact slide role, composition, safe area, and negative prompt.
- Shorten or split a slide when the image model cannot render dense text legibly.
- Update `qa/qa.md` after every fix so the final PDF has an auditable trail.

## PDF Final Check

- Page count equals the approved slide count.
- Page order matches `storyboard.md`.
- All pages have the same 16:9 dimensions.
- No slide has accidental padding, letterboxing, local overlays, or cropping.
- The first page works as a cover and the last page works as a confident ending.
