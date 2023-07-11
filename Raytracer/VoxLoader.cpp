#include "VoxLoader.h"
#define OGT_VOX_IMPLEMENTATION
#include <fstream>
#include "ogt_vox.h"
#include <vector>
void loadVox(const char* filename, std::function<void(int x, int y, int z, glm::uvec3 color)> emit) {
	std::fstream file(filename, std::ios::in | std::ios::binary);
	std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(file), {});

	const ogt_vox_scene* scene = ogt_vox_read_scene(buffer.data(), buffer.size());
	for (int i = 0; i < scene->num_models; ++i)
	{
		auto model = scene->models[i];
		for (int x = 0; x < model->size_x;++x)
			for (int y = 0; y < model->size_y;++y)
				for (int z = 0; z < model->size_z;++z) {
					auto data = model->voxel_data[ x+y*model->size_x + z*model->size_x	*model->size_y ]; // x->y->z order
					auto rgb = scene->palette.color[data];
					if (data) emit(x, z, y, { rgb.r, rgb.g, rgb.b });
				}
	}
}
