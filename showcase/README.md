# Builds & Projects showcase

A static page that lists our builds and projects, each with a short note and links. It deploys to Vercel with no build step.

## Add or update a project

Edit `projects.js`: copy any block, fill it in and push. Cards sort automatically: in progress first, then live, shipped and planned, newest first within each group.

```js
{
  name: "New thing",
  category: "hardware",   // hardware | client | apps | games | pitch | demos
  status: "building",     // live | building | shipped | idea
  updated: "2026-11",
  tags: ["ESP32"],
  info: "One or two sentences on what it is.",
  links: [{ label: "Live", url: "https://..." }, { label: "Code", url: GH("repo") }],
},
```

## Deploy

On Vercel, import this repo and set **Root Directory** to `showcase` (Framework preset: Other, no build command). Every push redeploys it.

Preview locally with `npx serve showcase` or by opening `index.html`.
