#include "FileSelector.h"

std::optional<File::path> FileSelector::show()
{
	if (!mOpen)
		return std::nullopt;
	std::vector<File::directory_entry> entries;
	File::directory_iterator list(mCurrent);

	for (const auto& itr : list)
		entries.push_back(itr);

	File::directory_entry selectedEntry;

	if (ImGui::Begin("File Selector", &mOpen))
	{
		ImGui::Text("%s", File::absolute(mCurrent.path()).generic_string().c_str());
		ImGui::SameLine();
		if (ImGui::Button("Up"))
			mCurrent = File::directory_entry(mCurrent.path().parent_path());

		if (ImGui::BeginChild("Child"))
		{
			for (const auto& entry : entries)
			{
				bool selected = false;
				auto relative = File::relative(entry.path(), mCurrent.path());
				if (ImGui::Selectable(relative.generic_string().c_str(), &selected))
					selectedEntry = entry;
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	if (!selectedEntry.exists())
		return std::nullopt;
	if (selectedEntry.is_directory())
	{
		mCurrent = selectedEntry;
		return std::nullopt;
	}
	else if (selectedEntry.is_regular_file())
		return selectedEntry.path();
	return std::nullopt;
}