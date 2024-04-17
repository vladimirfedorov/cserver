---
title    : Hello!
author   : B
category : Example
tags     : demo page
published: 2024-01-11 20:00:00
---

# Hello Page

## Site configuration:

{{#config}}
Title: {{title}}
Keywords: {{keywords}}
{{/config}}

## Site metadata:

{{#site}}
{{.}}
{{/site}}

## Page metadata:

{{#page}}
Page title: {{title}}
Page author: {{author}}
Category: {{category}}
Tags: {{tags}}
Slug: {{slug}}
Template: {{template}}
Publish date: {{published}}
{{/page}}
