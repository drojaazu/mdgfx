/**
 * @file filesys.hpp
 * @author Motoi Productions (Damian Rogers damian@motoi.pro)
 * @brief File system/path utilities
 *
 * Updates:
 * 20211214 Initial
 * 20220420 Converted to basic_string<type>, added path parsing functions
 */

#ifndef __MOTOI__FILESYS_HPP
#define __MOTOI__FILESYS_HPP

#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>

template <typename StringT>
struct stat stat(std::basic_string<StringT> const & path)
{
	static struct stat status;
	if(::stat(path, &status) != 0)
	{
		std::basic_stringstream<StringT> ss;
		ss << "Could not open path " << path << ": " << strerror(errno);
		throw runtime_error(ss.str());
	}
	return status;
}

template <typename StringT> bool exists(std::basic_string<StringT> const & path)
{
	static struct stat status;
	return (::stat(path.c_str(), &status) == 0);
}

template <typename StringT>
std::ifstream ifstream_checked(std::basic_string<StringT> const & path)
{
	std::ifstream ifs(path);
	if(!ifs.good())
		throw std::runtime_error(strerror(errno));
	return ifs;
}

template <typename StringT>
std::ofstream ofstream_checked(std::basic_string<StringT> const & path)
{
	std::ofstream ofs(path);
	if(!ofs.good())
		throw std::runtime_error(strerror(errno));
	return ofs;
}

template <typename StringT>
std::basic_string<StringT>
strip_extension(std::basic_string<StringT> const & path)
{
	auto i_at { path.find_last_of('.') };
	if(i_at == std::string::npos)
		return path;
	return path.substr(0, i_at);
}

template <typename StringT>
std::basic_string<StringT>
filename_from_path(std::basic_string<StringT> const & path)
{
	auto i_at { path.find_last_of('/') };
	if(i_at == std::string::npos)
		return path;
	return path.substr(0, i_at);
}

#endif
