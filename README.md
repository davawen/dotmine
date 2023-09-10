# Dotmine, a dotfiles manager

**Please note, this project is very much WIP, almost none of this is actually implemented yet! This is more like a spec for me to follow!**

## Reasoning

I currently use GNU stow to manage my dotfiles, and while the simplicity is nice, it often involves annoying manual work when bringing dotfiles to another computer and handling conflicts and adding new files.  
My primary goal was for there to be the least amount of manual work to do, and to be able to control at what level directories would get symlinked (stow simply symlinks at the lowest level possible, which again tends to be annoying when bringing files to another computer).  
This could probably have been cobbled together in a couple bash scripts, but I needed an excuse to start a C project, so here I am.

## Usage

To add a file or a directory to your dotfiles folder, use the `add` command.  
By default, `add` will directly symlink any directory you pass to it.
If you instead wish to symlink every inner file, use the `--recursive` flag:
```
$ dotmine add ~/dir

dir      |     |  dir -> ~/dotmine/dir
└file1   | ==> |
└file2   |     |

$ dotmine add --recursive ~/dir

dir      |     |  dir
└file1   | ==> |  └file1 -> ~/dotmine/dir/file1
└file2   |     |  └file2 -> ~/dotmine/dir/file2
```

TODO: stow

TODO: status

## Build and install

This is a standard meson project:
```bash
meson setup build --buildtype=release
ninja -C build
sudo ninja -C build install
```
