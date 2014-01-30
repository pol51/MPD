/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h" /* must be first for large file support */
#include "Walk.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseLock.hxx"
#include "db/Directory.hxx"
#include "db/Song.hxx"
#include "decoder/DecoderPlugin.hxx"
#include "decoder/DecoderList.hxx"
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagBuilder.hxx"
#include "Log.hxx"

#include <sys/stat.h>

#include <glib.h>

Directory *
UpdateWalk::MakeDirectoryIfModified(Directory &parent, const char *name,
				    const struct stat *st)
{
	Directory *directory = parent.FindChild(name);

	// directory exists already
	if (directory != nullptr) {
		if (directory->mtime == st->st_mtime && !walk_discard) {
			/* not modified */
			return nullptr;
		}

		editor.DeleteDirectory(directory);
		modified = true;
	}

	directory = parent.MakeChild(name);
	directory->mtime = st->st_mtime;
	return directory;
}

static bool
SupportsContainerSuffix(const DecoderPlugin &plugin, const char *suffix)
{
	return plugin.container_scan != nullptr &&
		plugin.SupportsSuffix(suffix);
}

bool
UpdateWalk::UpdateContainerFile(Directory &directory,
				const char *name, const char *suffix,
				const struct stat *st)
{
	const DecoderPlugin *_plugin = decoder_plugins_find([suffix](const DecoderPlugin &plugin){
			return SupportsContainerSuffix(plugin, suffix);
		});
	if (_plugin == nullptr)
		return false;
	const DecoderPlugin &plugin = *_plugin;

	db_lock();
	Directory *contdir = MakeDirectoryIfModified(directory, name, st);
	if (contdir == nullptr) {
		/* not modified */
		db_unlock();
		return true;
	}

	contdir->device = DEVICE_CONTAINER;
	db_unlock();

	const auto pathname = map_directory_child_fs(directory, name);

	char *vtrack;
	unsigned int tnum = 0;
	TagBuilder tag_builder;
	while ((vtrack = plugin.container_scan(pathname.c_str(), ++tnum)) != nullptr) {
		Song *song = Song::NewFile(vtrack, *contdir);

		// shouldn't be necessary but it's there..
		song->mtime = st->st_mtime;

		const auto child_path_fs =
			map_directory_child_fs(*contdir, vtrack);

		plugin.ScanFile(child_path_fs.c_str(),
				add_tag_handler, &tag_builder);

		tag_builder.Commit(song->tag);

		db_lock();
		contdir->AddSong(song);
		db_unlock();

		modified = true;

		FormatDefault(update_domain, "added %s/%s",
			      directory.GetPath(), vtrack);
		g_free(vtrack);
	}

	if (tnum == 1) {
		db_lock();
		editor.DeleteDirectory(contdir);
		db_unlock();
		return false;
	} else
		return true;
}