# Category index

#### List from `site.index.category`:

{{#site.index.category}}
- [{{title}}](/category/{{name}})
	{{#pages}}
	- [{{title}}](/{{link}})
	{{/pages}}
{{/site.index.category}}

#### List from `references.category`:

{{#references.category}}
- [{{title}}](/category/{{name}})
	{{#pages}}
	- [{{title}}](/{{link}})
	{{/pages}}
{{/references.category}}
