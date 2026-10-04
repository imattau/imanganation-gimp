# Sample project manifest

The `extension-imanganation-panels` sample plug-in stores its current project
state in `imanganation/blades-of-fate.project` under the user's GIMP
configuration directory. The file uses GLib's key-file format and is created
with sample data the first time the persistent plug-in starts.

```ini
[Project]
format-version=1
title=Blades of Fate
chapter=Chapter 04
first-page=14
page-count=5
current-page=16

[Assets]
characters=Mei Lin;Xiu Ying;
locations=Temple Courtyard;
props=Moonblade;
references=Visual guide;
```

Page additions and page selection update `page-count` and `current-page` and
are written back to this manifest. Asset labels are grouped into the four
listed categories and shown in the Project panel. Selecting one displays a
basic category view in the Inspector.

The Project panel's **Open Project…** action opens another `.project` manifest.
After a successful open, page additions and selection are saved back to that
file. The **＋ Add Page** tree item appends a page to the current range.

The plug-in starts with one sample project per GIMP user profile and can open
one active project manifest at a time. Page numbers range from 1 to 999999,
with up to 99 pages. Each asset category can contain
up to 450 labels; labels are limited to 256 bytes and cannot contain tabs or
newlines. The plug-in reads the manifest at startup, so stop GIMP before
editing it by hand. This prototype stores project metadata only; it does not
yet create page image files or associate existing GIMP images with pages.
There is no project creation dialog yet; to start another project, make a copy
of a manifest and edit its title, chapter, page range, and asset lists before
opening it.
