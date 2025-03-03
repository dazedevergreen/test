#include "DianaAimHardwareService.h"

lancetAlgorithm::DianaAimHardwareService::DianaAimHardwareService()
{
	m_TBaseRF2Base = vtkSmartPointer<vtkMatrix4x4>::New();
	m_TFlange2EndRF = vtkSmartPointer<vtkMatrix4x4>::New();
}

lancetAlgorithm::DianaAimHardwareService::~DianaAimHardwareService()
{
	m_ReferenceMap.clear();
	m_ToolTipMap.clear();
	m_LabelMap.clear();
}

void lancetAlgorithm::DianaAimHardwareService::ConnectCamera()
{
	T_AIMPOS_DATAPARA mPosDataPara;
	Aim_API_Initial(m_AimHandle);
	Aim_SetEthernetConnectIP(m_AimHandle, 192, 168, 31, 10);
	rlt = Aim_ConnectDevice(m_AimHandle, I_ETHERNET, mPosDataPara);

	if (rlt == AIMOOE_OK)
	{
		std::cout << "connect success";
	}
	else {

		std::cout << "connect failed" << std::endl;
		//return;
	}

	QString filename = QFileDialog::getExistingDirectory(nullptr, "Select the Tools store folder", "");
	if (filename.isNull()) return;
	filename.append("/");
	std::cout << "The selected folder address :" << filename.toStdString() << std::endl;;
	rlt = Aim_SetToolInfoFilePath(m_AimHandle, filename.toLatin1().data());

	if (rlt == AIMOOE_OK)
	{

		std::cout << "set filenemae success" << std::endl;
	}
	else {

		std::cout << "set filenemae failed" << std::endl;
		//return;
	}

	int size = 0;
	Aim_GetCountOfToolInfo(m_AimHandle, size);

	if (size != 0)
	{
		t_ToolBaseInfo* toolarr = new t_ToolBaseInfo[size];

		rlt = Aim_GetAllToolFilesBaseInfo(m_AimHandle, toolarr);

		if (rlt == AIMOOE_OK)
		{
			for (int i = 0; i < size; i++)
			{
				/*		char* ptool = toolarr[i].name;
						QString toolInfo = QString("Tool Name：") + QString::fromLocal8Bit(ptool);
						m_Controls.textBrowser->append(toolInfo);*/
			}
		}
		delete[] toolarr;
	}
	else {
		std::cout << "There are no tool identification files in the current directory:";
	}

	std::cout << "End of connection";
	rlt = AIMOOE_OK;
}

void lancetAlgorithm::DianaAimHardwareService::StartCamera()
{
	if (rlt!= AIMOOE_OK)
	{
		std::cout << "Please add Aim files first and check device connected" << std::endl;
		return;
	}
	std::cout << "Start Camera" << std::endl;
	if (m_ReferenceMap.size() <= 0)
		return;
	if (m_AimoeVisualizeTimer == nullptr)
	{
		m_AimoeVisualizeTimer = new QTimer(this);
	}

	connect(m_AimoeVisualizeTimer, &QTimer::timeout, [this]() {
		this->UpdateCamera();
		});
	m_AimoeVisualizeTimer->start(100);
}

void lancetAlgorithm::DianaAimHardwareService::UpdateCamera()
{
	QString position_text;
	std::vector<std::string> toolidarr;

	auto prlt = GetNewToolData();
	if (rlt == AIMOOE_OK)//判断是否采集成功
	{
		do
		{
			for (auto it = m_ReferenceMap.begin(); it != m_ReferenceMap.end(); ++it)
			{
				UpdateCameraToToolMatrix(prlt, it->first.c_str());
			}

			T_AimToolDataResult* pnext = prlt->next;
			delete prlt;
			prlt = pnext;
		} while (prlt != NULL);
	}
	else
	{
		delete prlt;
	}
	emit CameraUpdateClock();
}

void lancetAlgorithm::DianaAimHardwareService::InitToolsName(std::vector<std::string> toolsName, std::vector<QLabel*>* labels)
{
	if (m_ReferenceMap.size() > 0)
	{
		m_ReferenceMap.clear();
		m_ToolTipMap.clear();
		m_LabelMap.clear();
	}
	for (int i = 0; i < toolsName.size(); ++i)
	{
		if (m_ReferenceMap.count(toolsName[i]) > 0)
			continue;
		vtkSmartPointer<vtkMatrix4x4> m = vtkSmartPointer<vtkMatrix4x4>::New();
		Eigen::Vector3d tip(0, 0, 0);
		m_ReferenceMap.emplace(std::pair(toolsName[i], m));
		m_ToolTipMap.emplace(std::pair(toolsName[i], tip));
		if (labels && i < labels->size())
		{
			m_LabelMap.emplace(std::pair(toolsName[i], (*labels)[i]));
		}
		else
		{
			m_LabelMap.emplace(std::pair(toolsName[i], nullptr));
		}
	}
}

Eigen::Vector3d lancetAlgorithm::DianaAimHardwareService::GetTipByName(std::string aToolName)
{
	return m_ToolTipMap[aToolName];
}

vtkSmartPointer<vtkMatrix4x4> lancetAlgorithm::DianaAimHardwareService::GetMatrixByName(std::string aToolName)
{
	vtkSmartPointer<vtkMatrix4x4> ret = vtkSmartPointer<vtkMatrix4x4>::New();
	ret->DeepCopy(m_ReferenceMap[aToolName]);
	return ret;
}

void lancetAlgorithm::DianaAimHardwareService::ConnectRobot()
{
	destroySrv();
	srv_net_st* pinfo = new srv_net_st();
	memset(pinfo->SrvIp, 0x00, sizeof(pinfo->SrvIp));
	memcpy(pinfo->SrvIp, m_RobotIpAddress, strlen(m_RobotIpAddress));
	pinfo->LocHeartbeatPort = 0;
	pinfo->LocRobotStatePort = 0;
	pinfo->LocSrvPort = 0;
	int ret = initSrv(nullptr, nullptr, pinfo);
	if (ret < 0)
	{
		std::cout << "Couldn't connect Robot, please ping 192.168.10.75 first" << std::endl;
	}
	if (pinfo)
	{
		delete pinfo;
		pinfo = nullptr;
	}
}

void lancetAlgorithm::DianaAimHardwareService::RobotPowerOn()
{
	releaseBrake();
}

void lancetAlgorithm::DianaAimHardwareService::RobotPowerOff()
{
	holdBrake();
}

void lancetAlgorithm::DianaAimHardwareService::SetTCP2Flange()
{
	double pose[6] = { 0,0,0,0,0,0 };
	setDefaultActiveTcpPose(pose, m_RobotIpAddress);

	getTcpPos(pose, m_RobotIpAddress);
}

void lancetAlgorithm::DianaAimHardwareService::RecordIntialPos()
{
	getTcpPos(m_InitialPos, m_RobotIpAddress);
}

void lancetAlgorithm::DianaAimHardwareService::Translate(const double x, const double y, const double z)
{
	double pose[6] = {0.0};
	vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
	vtkSmartPointer<vtkMatrix4x4> vtkMatrix = vtkSmartPointer<vtkMatrix4x4>::New();


	vtkMatrix->DeepCopy(this->GetBase2TCP());
	PrintDataHelper::CoutMatrix("Current Base2TCP Matrix:", vtkMatrix);

	vtkMatrix->DeepCopy(this->GetBase2TCP());

	transform->SetMatrix(vtkMatrix);
	transform->Translate(x, y, z);

	vtkSmartPointer<vtkMatrix4x4> setMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
	transform->Update();
	transform->GetMatrix(setMatrix);

	this->RobotTransformInBase(setMatrix->GetData());
}

void lancetAlgorithm::DianaAimHardwareService::Translate(const double axis[3], double length)
{
	Translate(axis[0] * length, axis[1] * length, axis[2] * length);
}

void lancetAlgorithm::DianaAimHardwareService::Rotate(double x, double y, double z, double angle)
{
	double pose[6] = {};

	vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
	vtkSmartPointer<vtkMatrix4x4> vtkMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
	vtkMatrix->DeepCopy(this->GetBase2TCP());

	PrintDataHelper::CoutMatrix("Current Base2TCP Matrix:", vtkMatrix);

	vtkMatrix->Transpose();
	transform->SetMatrix(vtkMatrix);
	transform->RotateWXYZ(angle, x, y, z);

	vtkSmartPointer<vtkMatrix4x4> setMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
	transform->Update();
	transform->GetMatrix(setMatrix);
	
	this->RobotTransformInBase(setMatrix->GetData());
}

void lancetAlgorithm::DianaAimHardwareService::Rotate(const double axis[3], double angle)
{
	Rotate(axis[0], axis[1], axis[2], angle);
}

void lancetAlgorithm::DianaAimHardwareService::WaitMove(const char* m_RobotIpAddress)
{
	QThread::msleep(20);
	while (true)
	{
		const char state = getRobotState(m_RobotIpAddress);
		if (state != 0)
		{
			break;
		}
		else
		{
			QThread::msleep(1);
			QApplication::processEvents();
		}
	}
	stop();
}

void lancetAlgorithm::DianaAimHardwareService::RobotTransformInTCP(const double* matrix)
{
	double pose[6] = {};
	vtkSmartPointer<vtkMatrix4x4> TBase2Tcp = vtkSmartPointer<vtkMatrix4x4>::New();
	TBase2Tcp->DeepCopy(this->GetBase2TCP());
	PrintDataHelper::CoutMatrix("Current Base2TCP Matrix:", TBase2Tcp);
	vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
	transform->PreMultiply();
	transform->SetMatrix(TBase2Tcp);
	vtkSmartPointer<vtkMatrix4x4> vtkMatrix = vtkSmartPointer<vtkMatrix4x4>::New();

	vtkMatrix->DeepCopy(matrix);
	transform->Concatenate(vtkMatrix);

	vtkSmartPointer<vtkMatrix4x4> newBase2TCP = vtkSmartPointer<vtkMatrix4x4>::New();
	transform->GetMatrix(newBase2TCP);
	this->RobotTransformInBase(newBase2TCP->GetData());
}

void lancetAlgorithm::DianaAimHardwareService::RobotTransformInBase(const double* matrix)
{
	double pose[6] = {};
	vtkSmartPointer<vtkMatrix4x4> vtkMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
	vtkMatrix->DeepCopy(matrix);
	vtkMatrix->SetElement(0, 3, vtkMatrix->GetElement(0, 3) / 1000);
	vtkMatrix->SetElement(1, 3, vtkMatrix->GetElement(1, 3) / 1000);
	vtkMatrix->SetElement(2, 3, vtkMatrix->GetElement(2, 3) / 1000);
	vtkMatrix->Transpose();
	homogeneous2Pose(vtkMatrix->GetData(), pose);
	double joints_final[7]{};
	inverse(pose, joints_final, nullptr, m_RobotIpAddress);

	PrintDataHelper::CoutArray(joints_final, 7, "RobotTransformInBase joints_final");
	moveJToTarget(joints_final, 0.2, 0.4);
	WaitMove(m_RobotIpAddress);
}

void lancetAlgorithm::DianaAimHardwareService::SetTCP(vtkMatrix4x4* flangeToTarget)
{
	double pose[6] = {};
	getTcpPos(pose, m_RobotIpAddress);

	vtkSmartPointer<vtkMatrix4x4> TFlange2TCPNew = vtkSmartPointer<vtkMatrix4x4>::New();
	TFlange2TCPNew->DeepCopy(flangeToTarget);

	TFlange2TCPNew->SetElement(0, 3, TFlange2TCPNew->GetElement(0, 3) / 1000);
	TFlange2TCPNew->SetElement(1, 3, TFlange2TCPNew->GetElement(1, 3) / 1000);
	TFlange2TCPNew->SetElement(2, 3, TFlange2TCPNew->GetElement(2, 3) / 1000);
	TFlange2TCPNew->Transpose();

	homogeneous2Pose(TFlange2TCPNew->GetData(), pose);

	//int ret = setDefaultActiveTcp(TFlange2TCPNew->GetData());
	int ret = setDefaultActiveTcpPose(pose);
	std::cout << "ret: " << ret << std::endl;
	std::string str = ret < 0 ? "Set TCP Failed" : "Set TCP Success";
	std::cout << str << std::endl;

	getTcpPos(pose, m_RobotIpAddress);
}

bool lancetAlgorithm::DianaAimHardwareService::SetVelocity(int vel)
{
	int ret = setVelocityPercentValue(vel, m_RobotIpAddress);
	return ret == 1 ? true : false;
}

int lancetAlgorithm::DianaAimHardwareService::GetVelocity()
{
	//TODO
	return 0;
}

void lancetAlgorithm::DianaAimHardwareService::StopMove()
{
	stop();
}

vtkSmartPointer<vtkMatrix4x4> lancetAlgorithm::DianaAimHardwareService::GetRobotBase2RobotEnd()
{
	double joints[7] = { 0.0 };
	int ret = getJointPos(joints, m_RobotIpAddress);
	double pose[6] = {};
	forward(joints, pose);
	double matrix[16] = {};
	pose2Homogeneous(pose, matrix);
	vtkSmartPointer<vtkMatrix4x4> base2End = vtkSmartPointer<vtkMatrix4x4>::New();
	base2End->DeepCopy(matrix);
	base2End->Transpose();
	base2End->SetElement(0, 3, base2End->GetElement(0, 3) * 1000);
	base2End->SetElement(1, 3, base2End->GetElement(1, 3) * 1000);
	base2End->SetElement(2, 3, base2End->GetElement(2, 3) * 1000);
	return base2End;
}

vtkSmartPointer<vtkMatrix4x4> lancetAlgorithm::DianaAimHardwareService::GetEnd2TCP()
{
	vtkSmartPointer<vtkMatrix4x4> TEnd2Base = vtkSmartPointer<vtkMatrix4x4>::New();
	TEnd2Base->DeepCopy(this->GetRobotBase2RobotEnd());
	TEnd2Base->Invert();
	vtkSmartPointer<vtkMatrix4x4> TBase2TCP = vtkSmartPointer<vtkMatrix4x4>::New();
	TBase2TCP->DeepCopy(this->GetBase2TCP());

	vtkSmartPointer<vtkMatrix4x4> TEnd2TCP = vtkSmartPointer<vtkMatrix4x4>::New();
	vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
	transform->PreMultiply();
	transform->SetMatrix(TEnd2Base);
	transform->Concatenate(TBase2TCP);
	transform->Update();
	transform->GetMatrix(TEnd2TCP);

	return TEnd2TCP;
}

vtkSmartPointer<vtkMatrix4x4> lancetAlgorithm::DianaAimHardwareService::GetBase2TCP()
{
	double pose[6] = {};
	getTcpPos(pose, m_RobotIpAddress);

	//printf(" forward succeed! Pose: %f, %f, %f, %f, %f, %f\n ", pose[0], pose[1], pose[2], pose[3], pose[4], pose[5]);
	double matrixArray[16] = {};
	pose2Homogeneous(pose, matrixArray);//轴角转齐次变换矩阵  获得baseToTCP
	vtkSmartPointer<vtkMatrix4x4> TBase2Tcp = vtkSmartPointer<vtkMatrix4x4>::New();
	TBase2Tcp->DeepCopy(matrixArray);
	TBase2Tcp->Transpose();
	TBase2Tcp->SetElement(0, 3, TBase2Tcp->GetElement(0, 3) * 1000);
	TBase2Tcp->SetElement(1, 3, TBase2Tcp->GetElement(1, 3) * 1000);
	TBase2Tcp->SetElement(2, 3, TBase2Tcp->GetElement(2, 3) * 1000);

	return TBase2Tcp;
}

double* lancetAlgorithm::DianaAimHardwareService::GetRobotImpeda()
{
	double* Impedaa = new double[7];
	double arrStiff[6] = {};
	double dblDamp = 0;

	if (getCartImpeda(arrStiff, &dblDamp) < 0)
	{
		std::cout << "Get CartImpeda Failed" << std::endl;
	}

	for (int i = 0; i < 6; i++)
	{
		Impedaa[i] = arrStiff[i];
	}
	Impedaa[6] = dblDamp;
	return Impedaa;
}

bool lancetAlgorithm::DianaAimHardwareService::SetRobotImpeda(double* aData)
{
	double arrstiff[6] = {};
	double dblDamp = 0;
	for (int i = 0; i < 6; ++i)
	{
		arrstiff[i] = aData[i];
	}
	dblDamp = aData[6];
	int ret = setCartImpeda(arrstiff, dblDamp);

	return ret < 0 ? false : true;
}

bool lancetAlgorithm::DianaAimHardwareService::CleanRobotErrorInfo()
{
	int ret = cleanErrorInfo();
	return ret < 0 ? false : true;
}

std::vector<double> lancetAlgorithm::DianaAimHardwareService::GetJointsElectricCurrent()
{
	double joints[7] = { 0.0 };

	int ret = getJointCurrent(joints);
	std::vector<double> electricCurrent;
	for (int i = 0; i < 7; ++i)
	{
		electricCurrent.push_back(joints[i]);
	}
	return electricCurrent;
}

bool lancetAlgorithm::DianaAimHardwareService::SetPositionMode()
{
	int ret = changeControlMode(T_MODE_POSITION);
	return ret < 0 ? false : true;
}

bool lancetAlgorithm::DianaAimHardwareService::SetJointImpendanceMode()
{
	int ret = changeControlMode(T_MODE_JOINT_IMPEDANCE);
	return ret < 0 ? false : true;
}

bool lancetAlgorithm::DianaAimHardwareService::SetCartImpendanceMode()
{
	int ret = changeControlMode(T_MODE_CART_IMPEDANCE);
	return ret < 0 ? false : true;
}

std::vector<std::vector<double>> lancetAlgorithm::DianaAimHardwareService::GetJointsPositionRange()
{
	double dblMinPos[7] = { 0 }, dblMaxPos[7] = { 0 };
	int ret = getJointsPositionRange(dblMinPos, dblMaxPos);
	std::vector<std::vector<double>> range;
	std::vector<double> minRange;
	std::vector<double> maxRange;
	for (int i = 0; i < 7; ++i)
	{
		minRange.push_back(dblMinPos[i]);
		maxRange.push_back(dblMaxPos[i]);
	}
	range.push_back(minRange);
	range.push_back(maxRange);
	return range;
}

std::vector<double> lancetAlgorithm::DianaAimHardwareService::GetJointAngles()
{
	double pose[6] = {};
	getTcpPos(pose, m_RobotIpAddress);
	double angles[7] = { 0.0 };

	inverse(pose, angles, nullptr, m_RobotIpAddress);
	std::vector<double> angleVec;
	for (int i = 0; i < 7; ++i)
	{
		angleVec.push_back(angles[i]);
	}
	return angleVec;
}

bool lancetAlgorithm::DianaAimHardwareService::SetJointAngles(double* angles)
{
	auto range = this->GetJointsPositionRange();
	for (int i = 0; i < range[0].size(); ++i)
	{
		if (angles[i] < range[0][i] || angles[i] > range[1][i])
		{
			std::cout << "Joint Angle is beyond the limit" << std::endl;
			return false;
		}
	}
	moveJToTarget(angles, 0.2, 0.4);
	return true;
}

vtkSmartPointer<vtkMatrix4x4> lancetAlgorithm::DianaAimHardwareService::GetPositionPosByJointAngles(double* aDirection, double aLength)
{
	double pose[6] = {0.0};
	forward(m_RobotJointAngles, pose);
	double matrix[16] = {};
	pose2Homogeneous(pose, matrix);

	vtkSmartPointer<vtkMatrix4x4> vtkMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
	vtkMatrix->DeepCopy(matrix);
	vtkMatrix->Transpose();
	vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
	transform->SetMatrix(matrix);

	for (int i = 0; i < 3; ++i)
	{
		aDirection[i] = aDirection[i] * aLength / 1000;
	}

	transform->Translate(aDirection);

	vtkSmartPointer<vtkMatrix4x4> ret = vtkSmartPointer<vtkMatrix4x4>::New();
	transform->GetMatrix(ret);
	
	this->RobotTransformInBase(ret->GetData());
	return ret;
}

void lancetAlgorithm::DianaAimHardwareService::CapturePose(bool translationOnly)
{
	vtkSmartPointer<vtkMatrix4x4> TBase2Flange = vtkSmartPointer<vtkMatrix4x4>::New();
	
	TBase2Flange->DeepCopy(this->GetRobotBase2RobotEnd());

	double cameraToRoboEndArrayAvg[16];
	double cameraToBaseRFarrayAvg[16];

	/*AverageNavigationData("RobotEndRF", 100, 20, cameraToRoboEndArrayAvg);
	AverageNavigationData("RobotBaseRF", 100, 20, cameraToBaseRFarrayAvg);*/

	AverageNavigationData(cameraToRoboEndArrayAvg, cameraToBaseRFarrayAvg);

	vtkNew<vtkMatrix4x4> vtkNdiToRoboEndMatrix;
	vtkNew<vtkMatrix4x4> vtkBaseRFToNdiMatrix;
	vtkNdiToRoboEndMatrix->DeepCopy(cameraToRoboEndArrayAvg);
	vtkBaseRFToNdiMatrix->DeepCopy(cameraToBaseRFarrayAvg);
	vtkBaseRFToNdiMatrix->Invert();

	vtkNew<vtkTransform> tmpTransform;
	tmpTransform->PostMultiply();
	tmpTransform->Identity();
	tmpTransform->SetMatrix(cameraToRoboEndArrayAvg);
	tmpTransform->Concatenate(vtkBaseRFToNdiMatrix);
	tmpTransform->Update();
	auto vtkBaseRFtoRoboEndMatrix = tmpTransform->GetMatrix();

	m_RobotRegistration.AddPoseWithVtkMatrix(TBase2Flange, vtkBaseRFtoRoboEndMatrix, translationOnly);
}

bool lancetAlgorithm::DianaAimHardwareService::AverageNavigationData(std::string aRFName, int timeInterval, int intervalNum, double matrixArray[16])
{
	// The frame rate of Vega ST is 60 Hz, so the timeInterval should be larger than 16.7 ms
	if (timeInterval <= 16) {
		std::cout << "Time interval should be larger than 16.7 ms for 60 Hz frame rate." << std::endl;
		return false;
	}

	Eigen::Vector3d tmp_x = Eigen::Vector3d::Zero();
	Eigen::Vector3d tmp_y = Eigen::Vector3d::Zero();
	Eigen::Vector3d tmp_translation = Eigen::Vector3d::Zero();

	for (int i = 0; i < intervalNum; ++i) {
		this->UpdateCamera();
		vtkSmartPointer<vtkMatrix4x4> camera2RF = vtkSmartPointer<vtkMatrix4x4>::New();
		camera2RF->DeepCopy(this->GetMatrixByName(aRFName));

		tmp_x[0] += camera2RF->GetElement(0, 0);
		tmp_x[1] += camera2RF->GetElement(1, 0);
		tmp_x[2] += camera2RF->GetElement(2, 0);

		tmp_y[0] += camera2RF->GetElement(0, 1);
		tmp_y[1] += camera2RF->GetElement(1, 1);
		tmp_y[2] += camera2RF->GetElement(2, 1);

		tmp_translation[0] += camera2RF->GetElement(0, 3);
		tmp_translation[1] += camera2RF->GetElement(1, 3);
		tmp_translation[2] += camera2RF->GetElement(2, 3);

		QThread::msleep(timeInterval);
	}

	tmp_x /= intervalNum;
	tmp_y /= intervalNum;
	tmp_translation /= intervalNum;

	Eigen::Vector3d x = tmp_x.normalized();
	Eigen::Vector3d h = tmp_y.normalized();
	Eigen::Vector3d z = x.cross(h).normalized();
	Eigen::Vector3d y = z.cross(x).normalized();

	std::array<double, 16> tmpArray{
		x[0], y[0], z[0], tmp_translation[0],
		x[1], y[1], z[1], tmp_translation[1],
		x[2], y[2], z[2], tmp_translation[2],
		0,    0,    0,    1
	};

	std::copy(tmpArray.begin(), tmpArray.end(), matrixArray);
	return true;
}

void lancetAlgorithm::DianaAimHardwareService::AverageNavigationData(double camera2EndRF[16], double camera2BaseRF[16], int timeInterval, int intervalNum)
{
	if (timeInterval <= 16) {
		std::cout << "Time interval should be larger than 16.7 ms for 60 Hz frame rate." << std::endl;
		return;
	}

	Eigen::Vector3d endRFtmp_x = Eigen::Vector3d::Zero();
	Eigen::Vector3d endRFtmp_y = Eigen::Vector3d::Zero();
	Eigen::Vector3d endRFtmp_translation = Eigen::Vector3d::Zero();

	Eigen::Vector3d baseRFtmp_x = Eigen::Vector3d::Zero();
	Eigen::Vector3d baseRFtmp_y = Eigen::Vector3d::Zero();
	Eigen::Vector3d baseRFtmp_translation = Eigen::Vector3d::Zero();

	for (int i = 0; i < intervalNum; ++i) {
		this->UpdateCamera();
		vtkSmartPointer<vtkMatrix4x4> camera2endRF = vtkSmartPointer<vtkMatrix4x4>::New();
		vtkSmartPointer<vtkMatrix4x4> camera2baseRF = vtkSmartPointer<vtkMatrix4x4>::New();
		camera2endRF->DeepCopy(this->GetMatrixByName("RobotEndRF"));
		camera2baseRF->DeepCopy(this->GetMatrixByName("RobotBaseRF"));

		endRFtmp_x[0] += camera2endRF->GetElement(0, 0);
		endRFtmp_x[1] += camera2endRF->GetElement(1, 0);
		endRFtmp_x[2] += camera2endRF->GetElement(2, 0);

		endRFtmp_y[0] += camera2endRF->GetElement(0, 1);
		endRFtmp_y[1] += camera2endRF->GetElement(1, 1);
		endRFtmp_y[2] += camera2endRF->GetElement(2, 1);

		endRFtmp_translation[0] += camera2endRF->GetElement(0, 3);
		endRFtmp_translation[1] += camera2endRF->GetElement(1, 3);
		endRFtmp_translation[2] += camera2endRF->GetElement(2, 3);

		/********************************************************************************************/

		baseRFtmp_x[0] += camera2baseRF->GetElement(0, 0);
		baseRFtmp_x[1] += camera2baseRF->GetElement(1, 0);
		baseRFtmp_x[2] += camera2baseRF->GetElement(2, 0);
					
		baseRFtmp_y[0] += camera2baseRF->GetElement(0, 1);
		baseRFtmp_y[1] += camera2baseRF->GetElement(1, 1);
		baseRFtmp_y[2] += camera2baseRF->GetElement(2, 1);
		
		baseRFtmp_translation[0] += camera2baseRF->GetElement(0, 3);
		baseRFtmp_translation[1] += camera2baseRF->GetElement(1, 3);
		baseRFtmp_translation[2] += camera2baseRF->GetElement(2, 3);

		QThread::msleep(timeInterval);
	}

	endRFtmp_x /= intervalNum;
	endRFtmp_y /= intervalNum;
	endRFtmp_translation /= intervalNum;

	Eigen::Vector3d endRF_x = endRFtmp_x.normalized();
	Eigen::Vector3d endRF_h = endRFtmp_y.normalized();
	Eigen::Vector3d endRF_z = endRF_x.cross(endRF_h).normalized();
	Eigen::Vector3d endRF_y = endRF_z.cross(endRF_x).normalized();

	std::array<double, 16> endRFArray{
		endRF_x[0], endRF_y[0], endRF_z[0], endRFtmp_translation[0],
		endRF_x[1], endRF_y[1], endRF_z[1], endRFtmp_translation[1],
		endRF_x[2], endRF_y[2], endRF_z[2], endRFtmp_translation[2],
		0,    0,    0,    1
	};
	std::copy(endRFArray.begin(), endRFArray.end(), camera2EndRF);
	/*******************************************************************************************/
	baseRFtmp_x /= intervalNum;
	baseRFtmp_y /= intervalNum;
	baseRFtmp_translation /= intervalNum;

	Eigen::Vector3d baseRF_x = baseRFtmp_x.normalized();
	Eigen::Vector3d baseRF_h = baseRFtmp_y.normalized();
	Eigen::Vector3d baseRF_z = baseRF_x.cross(baseRF_h).normalized();
	Eigen::Vector3d baseRF_y = baseRF_z.cross(baseRF_x).normalized();

	std::array<double, 16> baseRFArray{
		baseRF_x[0], baseRF_y[0], baseRF_z[0], baseRFtmp_translation[0],
		baseRF_x[1], baseRF_y[1], baseRF_z[1], baseRFtmp_translation[1],
		baseRF_x[2], baseRF_y[2], baseRF_z[2], baseRFtmp_translation[2],
		0,    0,    0,    1
	};

	std::copy(baseRFArray.begin(), baseRFArray.end(), camera2BaseRF);
}

int lancetAlgorithm::DianaAimHardwareService::CaptureRobot()
{
	// 增加捕获姿态和更新UI的方法
	auto captureAndUpdateUI = [this](bool isTranslation) {
		CapturePose(isTranslation);
		int m_IndexOfRobotCapture = m_RobotRegistration.PoseCount();
		//m_Controls.CaptureCountLineEdit->setText(QString::number(m_IndexOfRobotCapture));
		MITK_INFO << "OnRobotCapture: " << m_IndexOfRobotCapture;
	};

	if (m_RobotRegistration.PoseCount() < 5) {
		captureAndUpdateUI(true); // 前五次捕获平移姿态
	}
	else if (m_RobotRegistration.PoseCount() < 10) {
		captureAndUpdateUI(false); // 后五次捕获旋转姿态
	}
	else {
		//MITK_INFO << "OnRobotCapture finish: " << m_IndexOfRobotCapture;
		vtkNew<vtkMatrix4x4> TFlange2EndRF;
		m_RobotRegistration.GetTCPmatrix(TFlange2EndRF);
		m_TFlange2EndRF->DeepCopy(TFlange2EndRF);
		vtkSmartPointer<vtkMatrix4x4> TBaseRF2Base = vtkSmartPointer<vtkMatrix4x4>::New();
		m_RobotRegistration.GetRegistraionMatrix(TBaseRF2Base);
		m_TBaseRF2Base->DeepCopy(TBaseRF2Base);

		double x = TFlange2EndRF->GetElement(0, 3);
		double y = TFlange2EndRF->GetElement(1, 3);
		double z = TFlange2EndRF->GetElement(2, 3);
		std::cout << "X: " << x << std::endl;
		std::cout << "Y: " << y << std::endl;
		std::cout << "Z: " << z << std::endl;

		std::cout << "Registration RMS: " << m_RobotRegistration.RMS() << std::endl;
	}
	return m_RobotRegistration.PoseCount();
}

int lancetAlgorithm::DianaAimHardwareService::ResetRobotRegistration()
{
	m_RobotRegistration.RemoveAllPose();
	return m_RobotRegistration.PoseCount();
	std::cout << "m_RobotRegistration Count: " << m_RobotRegistration.PoseCount() << std::endl;
}

void lancetAlgorithm::DianaAimHardwareService::GoToInitPos()
{
	double joints_final[7]{};
	inverse(m_InitialPos, joints_final, nullptr, m_RobotIpAddress);

	moveJToTarget(joints_final, 0.2, 0.4);
	WaitMove(m_RobotIpAddress);
}

void lancetAlgorithm::DianaAimHardwareService::RobotAutoRegistration()
{
	this->SetTCP2Flange();
	this->RecordIntialPos();

	double moveMent[][3] = {
		{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0,0,1}
	};

	for (; m_RobotRegistration.PoseCount() < 10; )
	{
		int count = m_RobotRegistration.PoseCount();
		if (count > 0)
		{
			GoToInitPos();
			QThread::msleep(200);
			QApplication::processEvents();
			WaitMove(m_RobotIpAddress);

		}
		if (m_RobotRegistration.PoseCount() < 5)
		{
			this->Translate(moveMent[count], 50);
			std::cout << "RobotMove " << count << " Down" << std::endl;
			QThread::msleep(200);
			QApplication::processEvents();
			WaitMove(m_RobotIpAddress);

			CapturePose(true);
			std::cout << "Capture " << count << " Down" << std::endl;
			//translation
		}
		else
		{
			this->Rotate(moveMent[count - 5], 15);
			std::cout << "RobotMove " << count << " Down" << std::endl;
			QThread::msleep(200);
			QApplication::processEvents();
			WaitMove(m_RobotIpAddress);

			CapturePose(false);
			std::cout << "Capture " << count << " Down" << std::endl;
			//rotation
		}
		QThread::msleep(200);
		QApplication::processEvents();
	}
	//Calculate Registration Data
	this->GoToInitPos();

	vtkNew<vtkMatrix4x4> TFlange2EndRF;
	m_RobotRegistration.GetTCPmatrix(TFlange2EndRF);
	m_TFlange2EndRF->DeepCopy(TFlange2EndRF);
	vtkSmartPointer<vtkMatrix4x4> TBaseRF2Base = vtkSmartPointer<vtkMatrix4x4>::New();
	m_RobotRegistration.GetRegistraionMatrix(TBaseRF2Base);
	m_TBaseRF2Base->DeepCopy(TBaseRF2Base);

	std::cout << "Registration RMS: " << m_RobotRegistration.RMS() << std::endl;
}

vtkSmartPointer<vtkMatrix4x4> lancetAlgorithm::DianaAimHardwareService::GetBaseRF2BaseMatrix()
{
	vtkSmartPointer<vtkMatrix4x4> ret = vtkSmartPointer<vtkMatrix4x4>::New();
	ret->DeepCopy(m_TBaseRF2Base);
	return ret;
}

vtkSmartPointer<vtkMatrix4x4> lancetAlgorithm::DianaAimHardwareService::GetEnd2EndRFMatrix()
{
	vtkSmartPointer<vtkMatrix4x4> ret = vtkSmartPointer<vtkMatrix4x4>::New();
	ret->DeepCopy(m_TFlange2EndRF);
	return ret;
}

void lancetAlgorithm::DianaAimHardwareService::SetBaseRF2BaseMatrix(vtkMatrix4x4* aMatrix)
{
	m_TBaseRF2Base->DeepCopy(aMatrix);
}

void lancetAlgorithm::DianaAimHardwareService::SetEnd2EndRFMatrix(vtkMatrix4x4* aMatrix)
{
	m_TFlange2EndRF->DeepCopy(aMatrix);
}

T_AimToolDataResult* lancetAlgorithm::DianaAimHardwareService::GetNewToolData()
{
	rlt = Aim_GetMarkerAndStatusFromHardware(m_AimHandle, I_ETHERNET, markerSt, statusSt);
	if (rlt == AIMOOE_NOT_REFLASH)
	{
		std::cout << "camera get data failed";
	}
	T_AimToolDataResult* mtoolsrlt = new T_AimToolDataResult;//新建一个值指，将指针清空用于存数据
	mtoolsrlt->next = NULL;
	mtoolsrlt->validflag = false;

	rlt = Aim_FindToolInfo(m_AimHandle, markerSt, mtoolsrlt, 0);//获取数据
	T_AimToolDataResult* prlt = mtoolsrlt;//将获取完数据的从mtoolsrlt给prlt指针

	return prlt;
}

bool lancetAlgorithm::DianaAimHardwareService::UpdateCameraToToolMatrix(T_AimToolDataResult* ToolData, const std::string Name)
{
	if (strcmp(ToolData->toolname, Name.c_str()) == 0)
	{
		auto label = m_LabelMap[Name];
		if (ToolData->validflag)
		{
			//获取相机数据
			Eigen::Vector3d camera2ToolTranslation;
			Eigen::Matrix3d camera2ToolRotation;
			camera2ToolTranslation[0] = ToolData->Tto[0];
			camera2ToolTranslation[1] = ToolData->Tto[1];
			camera2ToolTranslation[2] = ToolData->Tto[2];
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					camera2ToolRotation(i, j) = (double)ToolData->Rto[i][j];
				}
			}
			//拼接矩阵
			auto matrix = m_ReferenceMap[Name];
			
			matrix->DeepCopy(GetMatrixByRotationAndTranslation(camera2ToolRotation, camera2ToolTranslation));
			Eigen::Vector3d tip;
			tip[0] = ToolData->tooltip[0];
			tip[1] = ToolData->tooltip[1];
			tip[2] = ToolData->tooltip[2];
			m_ToolTipMap[Name] = tip;
			//memcpy_s(aCamera2Tool, sizeof(double) * 16, matrix->GetData(), sizeof(double) * 16);
			if (label != nullptr)
			{
				QString str = "x:" + QString::number(ToolData->tooltip[0]) + "\n "
					+ "y:" + QString::number(ToolData->tooltip[1]) + " \n "
					+ "z:" + QString::number(ToolData->tooltip[2]);
				label->setText(str);
				label->setStyleSheet("QLabel { color : green; }");
			}
			return true;
		}
		else
		{
			if (label != nullptr)
			{
				QString str = "x:nan  y:nan   z:nan";
				label->setText(str);
				label->setStyleSheet("QLabel { color : red; }");
			}
			return false;
		}
	}
	return false;
}

vtkSmartPointer<vtkMatrix4x4> lancetAlgorithm::DianaAimHardwareService::GetMatrixByRotationAndTranslation(Eigen::Matrix3d rotation, Eigen::Vector3d translation)
{
	vtkSmartPointer<vtkMatrix4x4> matrix = vtkSmartPointer<vtkMatrix4x4>::New();
	matrix->Identity();
	for (int row = 0; row < 3; row++)
	{
		for (int col = 0; col < 3; col++)
		{
			matrix->SetElement(row, col, rotation(row, col));
		}
	}
	matrix->SetElement(0, 3, translation[0]);
	matrix->SetElement(1, 3, translation[1]);
	matrix->SetElement(2, 3, translation[2]);
	return matrix;
}
