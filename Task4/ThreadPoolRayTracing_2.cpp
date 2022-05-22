#include "minirt/minirt.h"

#include <cmath>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>

using namespace minirt;

void initScene(Scene& scene) {
	Color red{ 1, 0.2, 0.2 };
	Color blue{ 0.2, 0.2, 1 };
	Color green{ 0.2, 1, 0.2 };
	Color white{ 0.8, 0.8, 0.8 };
	Color yellow{ 1, 1, 0.2 };

	Material metallicRed{ red, white, 50 };
	Material mirrorBlack{ Color {0.0}, Color {0.9}, 1000 };
	Material matteWhite{ Color {0.7}, Color {0.3}, 1 };
	Material metallicYellow{ yellow, white, 250 };
	Material greenishGreen{ green, 0.5, 0.5 };

	Material transparentGreen{ green, 0.8, 0.2 };
	transparentGreen.makeTransparent(1.0, 1.03);
	Material transparentBlue{ blue, 0.4, 0.6 };
	transparentBlue.makeTransparent(0.9, 0.7);

	scene.addSphere(Sphere{ {0, -2, 7}, 1, transparentBlue });
	scene.addSphere(Sphere{ {-3, 2, 11}, 2, metallicRed });
	scene.addSphere(Sphere{ {0, 2, 8}, 1, mirrorBlack });
	scene.addSphere(Sphere{ {1.5, -0.5, 7}, 1, transparentGreen });
	scene.addSphere(Sphere{ {-2, -1, 6}, 0.7, metallicYellow });
	scene.addSphere(Sphere{ {2.2, 0.5, 9}, 1.2, matteWhite });
	scene.addSphere(Sphere{ {4, -1, 10}, 0.7, metallicRed });

	scene.addLight(PointLight{ {-15, 0, -15}, white });
	scene.addLight(PointLight{ {1, 1, 0}, blue });
	scene.addLight(PointLight{ {0, -10, 6}, red });

	scene.setBackground({ 0.05, 0.05, 0.08 });
	scene.setAmbient({ 0.1, 0.1, 0.1 });
	scene.setRecursionLimit(20);

	scene.setCamera(Camera{ {0, 0, -20}, {0, 0, 0} });
}

////////////////////////////////////////////////////////////

class TaskBase {
public:
    virtual ~TaskBase() {}
    virtual bool isStop() const { return false; }
};

class ResultBase {
public:
    virtual ~ResultBase() {}
};

class TaskStop : public TaskBase {
public:
    bool isStop() const override { return true; }
};

class TaskProducerBase {
public:
    virtual ~TaskProducerBase() {}
    virtual TaskBase* produce() = 0; // must be thread-safe
};

class ResultConsumerBase {
public:
    
    virtual ~ResultConsumerBase() {}
    virtual void consume(std::shared_ptr<ResultBase> result) = 0; // must be thread-safe
};

class WorkerBase {
public:
    virtual ~WorkerBase() {}
    virtual ResultBase* run(std::shared_ptr<TaskBase> task) = 0;
};

class WorkerFactoryBase {
public:
    virtual ~WorkerFactoryBase() {}
    virtual WorkerBase* create() = 0;
};

class ThreadPool {
private:
    TaskProducerBase& producer;
    ResultConsumerBase& consumer;
    WorkerFactoryBase& workerFactory;

    void WorkerThread() {
        std::shared_ptr<WorkerBase> worker(workerFactory.create());
        for (;;) {
            std::shared_ptr<TaskBase> task(producer.produce());
            if (task->isStop())
                break;
            std::shared_ptr<ResultBase> result(worker->run(task));
            consumer.consume(result);
        }
    }

public:
    ThreadPool(
        TaskProducerBase& producer0,
        ResultConsumerBase& consumer0,
        WorkerFactoryBase& workerFactory0
    )
        : producer(producer0),
          consumer(consumer0),
          workerFactory(workerFactory0)
    {}

    void run(int workers_num) {
        std::vector<std::thread> worker_threads;
        for (int i = 0; i < workers_num; ++i)
            worker_threads.emplace_back(&ThreadPool::WorkerThread, this);
        for (int i = 0; i < workers_num; ++i)
            worker_threads[i].join();
    }
};


struct RayTracingTask : TaskBase {
	int x, y, w, h;
};

struct RayTracingResult : ResultBase {
	RayTracingTask task;
	std::vector<std::vector<Color>> pixels;
};

class RayTracingTaskProducer : public TaskProducerBase {
private:
	int width;
	int heigth;
	int gran;
	int posX;
	int posY;
	std::mutex m;

public:
	RayTracingTaskProducer(int width0, int heigth0, int gran0)
		: width(width0),
		  heigth(heigth0),
		  gran(gran0),
		  posX(0),
		  posY(0)
	{}

	TaskBase* produce() override {
		const std::lock_guard<std::mutex> lock(m);

		if (posY >= heigth)
			return new TaskStop();
		
		RayTracingTask *task = new RayTracingTask();

		task->x = posX;
		task->y = posY;
		task->w = std::min(gran, width - posX);
		task->h = std::min(gran, heigth - posY);
		
		posX += gran;
		if (posX >= width) {
			posX = 0;
			posY += gran;
		}

		return task;
	}
};

class RayTracingResultConsumer : public ResultConsumerBase {
private:
	Image &image;

public:
	RayTracingResultConsumer(Image &image0) : image(image0) {}

	void consume(std::shared_ptr<ResultBase> resultBase) {
        RayTracingResult* result = dynamic_cast<RayTracingResult*>(resultBase.get());
        if (!result)
            throw std::exception();
		
		auto task = result->task;
		for (int i = 0; i < task.w; ++i) {
			for (int j = 0; j < task.h; ++j) {
				image.set(task.x + i, task.y + j, result->pixels[i][j]);
			}
		}
	}
};

class RayTracingWorker : public WorkerBase {
private:
	ViewPlane &viewPlane;
	Scene &scene;
	int numOfSamples;

public:
	RayTracingWorker(ViewPlane &viewPlane0, Scene &scene0, int numOfSamples0)
		: viewPlane(viewPlane0),
		  scene(scene0),
		  numOfSamples(numOfSamples0)
	{}
	
	ResultBase* run(std::shared_ptr<TaskBase> taskBase) override {
        RayTracingTask* task = dynamic_cast<RayTracingTask*>(taskBase.get());
        if (!task)
            throw std::exception();
		
		RayTracingResult *result = new RayTracingResult();
		result->task = *task;
		result->pixels.resize(task->w);
		for (int i = 0; i < task->w; ++i) {
			result->pixels[i].resize(task->h);
			for (int j = 0; j < task->h; ++j) {
				result->pixels[i][j] = viewPlane.computePixel(scene, task->x + i, task->y + j, numOfSamples);
			}
		}
		return result;
	}
};

class RayTracingWorkerFactory : public WorkerFactoryBase {
private:
	ViewPlane &viewPlane;
	Scene &scene;
	int numOfSamples;

public:
	RayTracingWorkerFactory(ViewPlane &viewPlane0, Scene &scene0, int numOfSamples0)
		: viewPlane(viewPlane0),
		  scene(scene0),
		  numOfSamples(numOfSamples0)
	{}
	
	WorkerBase *create() override {
		return new RayTracingWorker(viewPlane, scene, numOfSamples);
	}	
};

////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
	int viewPlaneResolutionX = (argc > 1 ? std::stoi(argv[1]) : 1500);
	int viewPlaneResolutionY = (argc > 2 ? std::stoi(argv[2]) : 1500);
	int numOfSamples = (argc > 3 ? std::stoi(argv[3]) : 1);
	int granularity = (argc > 4 ? std::stoi(argv[4]) : 8);
	int num_threads = (argc > 5 ? std::stoi(argv[5]) : 1);
	std::string sceneFile = (argc > 6 ? argv[6] : "");

	Scene scene;
	if (sceneFile.empty()) {
		initScene(scene);
	}
	else {
		scene.loadFromFile(sceneFile);
	}

	const double backgroundSizeX = 4;
	const double backgroundSizeY = 4;
	const double backgroundDistance = 15;

	const double viewPlaneDistance = 5;
	const double viewPlaneSizeX = backgroundSizeX * viewPlaneDistance / backgroundDistance;
	const double viewPlaneSizeY = backgroundSizeY * viewPlaneDistance / backgroundDistance;

	ViewPlane viewPlane{ viewPlaneResolutionX, viewPlaneResolutionY, viewPlaneSizeX, viewPlaneSizeY, viewPlaneDistance };

	Image image(viewPlaneResolutionX, viewPlaneResolutionY);

#define THREAD_POOL
#ifdef THREAD_POOL
	RayTracingTaskProducer producer(viewPlaneResolutionX, viewPlaneResolutionY, granularity);
	RayTracingResultConsumer consumer(image);
	RayTracingWorkerFactory workerFactory(viewPlane, scene, numOfSamples);

	ThreadPool threadPool(producer, consumer, workerFactory);
	threadPool.run(num_threads);
#else
	for (int x = 0; x < viewPlaneResolutionX; x++)
	{
		for (int y = 0; y < viewPlaneResolutionY; y++)
		{
			const auto color = viewPlane.computePixel(scene, x, y, numOfSamples);
			image.set(x, y, color);
		}
	}
#endif

	image.saveJPEG("raytracing.jpg");

	return 0;
}
