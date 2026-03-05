#pragma once

struct GitHubReleaseInfo_s
{
	std::string tagName;
	std::string htmlUrl;
	std::string description;

	const char* GetHighestDifferingVersionNumber() const;
};

constexpr const char GITHUB_RELEASES_API_URL[] = "https://api.github.com/repos/r-ex/rsx/releases";

bool GetLatestGitHubReleaseInformation(GitHubReleaseInfo_s* update);